/*
 *  linux/drivers/ide/ps2dma.c
 *  PlayStation 2 IDE DMA driver
 *
 *	Copyright (C) 2001-2002  Sony Computer Entertainment Inc.
 *
 *  This file is subject to the terms and conditions of the GNU General
 *  Public License Version 2. See the file "COPYING" in the main
 *  directory of this archive for more details.
 *
 * Some code taken from drivers/ide/ide-dma.c:
 *
 *  Copyright (c) 1995-1998  Mark Lord
 *
 */

#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/ioport.h>
#include <linux/timer.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/completion.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/pgtable.h>

#include <asm/ps2/sifdefs.h>
#include <asm/ps2/dmarelay.h>
#include <asm/ps2/speed.h>

#define DMA_MAX_SEGMENTS	16

#define IFC_ATA_RST	0x80
#define IFC_DMA_EN	0x04

//#define DEBUG

#ifdef DEBUG
#define DPRINT(fmt, args...)	printk(__FILE__ ": " fmt, ## args)
#else
#define DPRINT(fmt, args...)	do {} while(0)
#endif

#undef NO_DMA_WRITE
#undef NO_DMA_READ
#undef GATHER_WRITE_DATA

extern char *ide_dmafunc_verbose(ide_dma_action_t dmafunc);

struct ps2_dmatable {
	unsigned int ata_iop_buffer __attribute__((aligned(64)));
	struct ata_dma_request ata_dma_request __attribute__((aligned(64)));
#ifdef GATHER_WRITE_DATA
	unsigned char *dma_buffer;
#endif
	ps2sif_clientdata_t cd_ata;
	ps2sif_clientdata_t cd_ata_end;
};

#ifdef GATHER_WRITE_DATA
static unsigned char dma_buffer[ATA_BUFFER_SIZE] __attribute__((aligned(64)));
#endif

/*
 * dma_intr() is the handler for disk read/write DMA interrupts
 */
ide_startstop_t ide_dma_intr (ide_drive_t *drive)
{
	int i;
	byte stat, dma_stat;

	DPRINT("ide_dma_intr\n");
	dma_stat = HWIF(drive)->dmaproc(ide_dma_end, drive);
	stat = GET_STAT();			/* get drive status */
	DPRINT("stat=%02x\n", stat);
	if (OK_STAT(stat,DRIVE_READY,drive->bad_wstat|DRQ_STAT)) {
		if (!dma_stat) {
			struct request *rq = HWGROUP(drive)->rq;
			rq = HWGROUP(drive)->rq;
			for (i = rq->nr_sectors; i > 0;) {
				i -= rq->current_nr_sectors;
				ide_end_request(1, HWGROUP(drive));
			}
			return ide_stopped;
		}
		printk("%s: dma_intr: bad DMA status\n", drive->name);
	}
	ide__sti();	/* local CPU only */
	return ide_error(drive, "dma_intr", stat);
}

/*
 * ps2_ide_build_dmatable() prepares a dma request.
 * Returns 0 if all went okay, returns 1 otherwise.
 */
static int ps2_ide_build_dmatable(int rw, ide_drive_t *drive)
{
	struct request *rq = HWGROUP(drive)->rq;
	struct buffer_head *bh = rq->bh;
	unsigned int size, addr;
	unsigned int count = 0, totalsize = 0;
	struct ps2_dmatable *t = (struct ps2_dmatable *)HWIF(drive)->dmatable_cpu;
	struct ata_dma_request *req = &t->ata_dma_request;
	unsigned int iopaddr = t->ata_iop_buffer;
#ifdef GATHER_WRITE_DATA
	unsigned char *dma_buffer = t->dma_buffer;
#endif

	DPRINT("nr_sectors %ld\n", rq->nr_sectors);
	do {
		/*
		 * Determine addr and size of next buffer area.  We assume that
		 * individual virtual buffers are always composed linearly in
		 * physical memory.  For example, we assume that any 8kB buffer
		 * is always composed of two adjacent physical 4kB pages rather
		 * than two possibly non-adjacent physical 4kB pages.
		 */
		if (bh == NULL) {  /* paging requests have (rq->bh == NULL) */
			addr = virt_to_bus (rq->buffer);
			size = rq->nr_sectors << 9;
		} else {
			/* group sequential buffers into one large buffer */
			addr = virt_to_bus (bh->b_data);
			size = bh->b_size;
			while ((bh = bh->b_reqnext) != NULL) {
				if ((addr + size) != virt_to_bus (bh->b_data))
					break;
				size += bh->b_size;
			}
		}
		/*
		 * Fill in the dma table.
		 * EE requires 128-bit alignment of all blocks,
		 */
		if ((addr & 0x0f)) {
			printk("%s: misaligned DMA buffer\n", drive->name);
			return 0;
		}
		if (count >= ATA_MAX_ENTRIES) {
			printk("%s: DMA table too small\n", drive->name);
			return 0; /* revert to PIO for this request */
		}
		DPRINT("ps2_ide_build_dmatable: %08x->%08x %d\n", addr, iopaddr, size);
#if !defined(GATHER_WRITE_DATA)
		if (rw) {	/* write */
			req->sdd[count].data = addr;
			req->sdd[count].addr = iopaddr;
		} else {	/* read */
			req->sdd[count].data = iopaddr;
			req->sdd[count].addr = addr;
		}
		req->sdd[count].size = size;
		req->sdd[count].mode = 0;
#else
		if (rw) {
			memcpy(dma_buffer, (void *)bus_to_virt(addr), size);
			dma_buffer += size;
		} else {
			req->sdd[count].data = iopaddr;
			req->sdd[count].addr = addr;
			req->sdd[count].size = size;
			req->sdd[count].mode = 0;
		}
#endif
		iopaddr += size;
		totalsize += size;
		count++;
	} while (bh != NULL);
	if (totalsize > ATA_BUFFER_SIZE) {
		printk("%s: DMA buffer too small\n", drive->name);
		return 0; /* revert to PIO for this request */
	}
	req->count = count;
	req->size = totalsize;
	if (!count)
		printk("%s: empty DMA table?\n", drive->name);
	return count;
}

int report_drive_dmaing (ide_drive_t *drive)
{
	struct hd_driveid *id = drive->id;

	if ((id->field_valid & 4) &&
	    (id->dma_ultra & (id->dma_ultra >> 11) & 7)) {
		if ((id->dma_ultra >> 13) & 1) {
			printk(", UDMA(100)");	/* UDMA BIOS-enabled! */
		} else if ((id->dma_ultra >> 12) & 1) {
			printk(", UDMA(66)");	/* UDMA BIOS-enabled! */
		} else {
			printk(", UDMA(44)");	/* UDMA BIOS-enabled! */
		}
	} else if ((id->field_valid & 4) &&
		   (id->dma_ultra & (id->dma_ultra >> 8) & 7)) {
		if ((id->dma_ultra >> 10) & 1) {
			printk(", UDMA(33)");	/* UDMA BIOS-enabled! */
		} else if ((id->dma_ultra >> 9) & 1) {
			printk(", UDMA(25)");	/* UDMA BIOS-enabled! */
		} else {
			printk(", UDMA(16)");	/* UDMA BIOS-enabled! */
		}
	} else if (id->field_valid & 4) {
		printk(", (U)DMA");	/* Can be BIOS-enabled! */
	} else {
		printk(", DMA");
	}
	return 1;
}

/*
 * ps2_ide_dmaproc() initiates/aborts DMA read/write operations on a drive.
 *
 * The caller is assumed to have selected the drive and programmed the drive's
 * sector address using CHS or LBA.  All that remains is to prepare for DMA
 * and then issue the actual read/write DMA/PIO command to the drive.
 *
 * Returns 0 if all went well.
 * Returns 1 if DMA read/write could not be started, in which case
 * the caller should revert to PIO for the current request.
 */

static int ps2_ide_dmaproc (ide_dma_action_t func, ide_drive_t *drive)
{
	ide_hwif_t *hwif = HWIF(drive);
	struct ps2_dmatable *t = (struct ps2_dmatable *)hwif->dmatable_cpu;
	struct ata_dma_request *req = &t->ata_dma_request;
	int ret;

#if !defined(GATHER_WRITE_DATA)
	ps2sif_dmadata_t *sdd;
	int cnt, i;
#endif /* !GATHER_WRITE_DATA */

	DPRINT("ps2_ide_dmaproc: %s\n", ide_dmafunc_verbose(func));
	switch (func) {
	case ide_dma_off:
		printk("%s: DMA disabled\n", drive->name);
	case ide_dma_off_quietly:
	case ide_dma_on:
		drive->using_dma = (func == ide_dma_on);
		return 0;
	case ide_dma_check:
		/* only ide-disk DMA works... */
		drive->using_dma = hwif->autodma && drive->media == ide_disk;

		/* TODO: always UltraDMA mode 4 */
		if (drive->using_dma) {
			ide_config_drive_speed(drive, XFER_UDMA_4);
		}
		return 0;

	case ide_dma_read:
#ifdef NO_DMA_READ
		return 1;
#endif
		if (drive->media != ide_disk)
			return 0;
		if (!ps2_ide_build_dmatable(0, drive))
			return 1;	/* try PIO instead of DMA */
		req->command = WIN_READDMA;
		req->devctrl = drive->ctl;
		drive->waiting_for_dma = 1;
		ide_set_handler(drive, &ide_dma_intr, WAIT_CMD, NULL);

		/* set nIEN for disable ATA interrupt */
		/* (ATA interrupt is enabled in RPC handler) */
		OUT_BYTE(drive->ctl|2, hwif->io_ports[IDE_CONTROL_OFFSET]);

		flush_cache_all();
		do {
			ret = ps2sif_callrpc(&t->cd_ata, SIFNUM_DmaRead,
					     SIF_RPCM_NOWAIT, (void *)req,
					     sizeof(int) * 4 + sizeof(ps2sif_dmadata_t) * req->count,
					     NULL, 0, NULL, NULL);
			switch (ret) {
			case 0:
				break;
			case -SIF_RPCE_SENDP:
				break;
			default:
				/* restore nIEN */
				OUT_BYTE(drive->ctl, hwif->io_ports[IDE_CONTROL_OFFSET]);

				printk("ps2_ide_dmaproc(read): callrpc failed, result=%d\n", ret);
				drive->waiting_for_dma = 0;
				return 1;
			}
		} while (ret < 0);
		return 0;

	case ide_dma_write:
#ifdef NO_DMA_WRITE
		return 1;
#endif
		if (drive->media != ide_disk)
			return 0;
		if (!ps2_ide_build_dmatable(1, drive))
			return 1;	/* try PIO instead of DMA */
		req->command = WIN_WRITEDMA;
		drive->waiting_for_dma = 1;
		ide_set_handler(drive, &ide_dma_intr, WAIT_CMD, NULL);

		flush_cache_all();
#if !defined(GATHER_WRITE_DATA)
		sdd = req->sdd;
		for (cnt = 0; cnt < req->count; cnt++) {
			while (ps2sif_setdma(sdd, 1) == 0) {
				i = 0x010000;
				while (i--)
					;
			}
			sdd++;
		}
#else	/* GATHER_WRITE_DATA */
		req->sdd[0].data = t->dma_buffer;
		req->sdd[0].addr = t->ata_iop_buffer;
		req->sdd[0].size = req->size;
		req->sdd[0].mode = 0;
		while (ps2sif_setdma(req->sdd, 1) == 0) {
			i = 0x010000;
			while (i--)
				;
		}
#endif
		do {
			ret = ps2sif_callrpc(&t->cd_ata, SIFNUM_DmaWrite,
					     SIF_RPCM_NOWAIT, (void *)req,
					     sizeof(int) * 4,
					     NULL, 0, NULL, NULL);
			switch (ret) {
			case 0:
				break;
			case -SIF_RPCE_SENDP:
				break;
			default:
				printk("ps2_ide_dmaproc(write): callrpc failed, result=%d\n", ret);
				drive->waiting_for_dma = 0;
				return 1;
			}
		} while (ret < 0);
		return 0;

	case ide_dma_begin:
		/* TODO */
		return 0;
	case ide_dma_end: /* returns 1 on error, 0 otherwise */
		/* disable DMA transfer */
		*SPD_R_XFR_CTRL = 0;
		*SPD_R_IF_CTRL = *SPD_R_IF_CTRL & ~IFC_DMA_EN;
		/* force break DMA */
		if (!(*SPD_R_INTR_STAT & 0x0001)) {
			unsigned char if_ctrl;
			if_ctrl = *SPD_R_IF_CTRL;
			*SPD_R_IF_CTRL = IFC_ATA_RST;
			udelay(100);
			*SPD_R_IF_CTRL = if_ctrl;
			do {
				ret = ps2sif_callrpc(&t->cd_ata_end, 0, SIF_RPCM_NOWAIT, NULL, 0, NULL, 0, NULL, NULL);
				switch (ret) {
				case 0:
					break;
				case -SIF_RPCE_SENDP:
					break;
				default:
					printk("ps2_ide_dmaproc(end): callrpc failed, result=%d\n", ret);
					break;
				}
			} while (ret == -SIF_RPCE_SENDP);
		}
		drive->waiting_for_dma = 0;
		return 0;
	case ide_dma_test_irq: /* returns 1 if dma irq issued, 0 otherwise */
		return (*SPD_R_INTR_STAT & 0x0001) ? 1 : 0;

	case ide_dma_bad_drive:
	case ide_dma_good_drive:
		return 0;
	case ide_dma_verbose:
		return report_drive_dmaing(drive);
	case ide_dma_timeout:
	case ide_dma_retune:
	case ide_dma_lostirq:
		return 0;

	default:
		printk("ps2_ide_dmaproc: unsupported func: %d\n", func);
		return 1;
	}
}

int ide_release_dma (ide_hwif_t *hwif)
{
	if (hwif->dmatable_cpu)
		kfree((void *)hwif->dmatable_cpu);
	return 1;
}

static void ps2dma_wakeup(void *p)
{
	complete((struct completion *)p);    
}

void __init ps2_ide_setup_dma (ide_hwif_t *hwif)
{
	struct ps2_dmatable *t;
	int ret;
	int i;
	int *dma_max_segments;
	DECLARE_COMPLETION(c);

	printk("PlayStation 2 IDE DMA driver");

	t = kmalloc(sizeof(struct ps2_dmatable), GFP_KERNEL);
	if (t == NULL)
		goto error;
	memset(t, 0, sizeof(struct ps2_dmatable));
#ifdef GATHER_WRITE_DATA
	t->dma_buffer = dma_buffer;
#endif

	/*
	 * bind to DMA relay module
	 */

	do {
		ret = ps2sif_bindrpc(&t->cd_ata, SIFNUM_ATA_DMA_BEGIN,
				     SIF_RPCM_NOWAIT, ps2dma_wakeup, (void *)&c);
		switch (ret) {
		case 0:
			break;
		case -SIF_RPCE_SENDP:
			break;
		default:
			printk("ps2_ide_setup_dma: bindrpc failed, result=%d\n", ret);
			goto error;
		}
	} while (ret < 0);
	wait_for_completion(&c);
	if (t->cd_ata.serve == 0)
		goto error;

	do {
		ret = ps2sif_bindrpc(&t->cd_ata_end, SIFNUM_ATA_DMA_END,
				     SIF_RPCM_NOWAIT, ps2dma_wakeup, (void *)&c);
		switch (ret) {
		case 0:
			break;
		case -SIF_RPCE_SENDP:
			break;
		default:
			printk("ps2_ide_setup_dma: bindrpc failed, result=%d\n", ret);
			goto error;
		}
	} while (ret < 0);
	wait_for_completion(&c);
	if (t->cd_ata_end.serve == 0)
		goto error;

	/*
	 * get IOP DMA buffer address
	 */
	do {
		ret = ps2sif_callrpc(&t->cd_ata, SIFNUM_GetBufAddr,
				     SIF_RPCM_NOWAIT, NULL, 0,
				     &t->ata_iop_buffer, sizeof(unsigned int),
				     ps2dma_wakeup, (void *)&c);
		switch (ret) {
		case 0:
			break;
		case -SIF_RPCE_SENDP:
			break;
		default:
			printk("ps2_ide_setup_dma: callrpc failed, result=%d\n", ret);
			goto error;
		}
	} while (ret < 0);
	wait_for_completion(&c);

	hwif->dma_base = 1;
	hwif->dmatable_cpu = (void *)t;
	hwif->dmaproc = &ps2_ide_dmaproc;
	if (!noautodma)
		hwif->autodma = 1;
	for (i = 0; i < MAX_DRIVES; i++)
		hwif->drives[i].keep_settings = 1;

	/* set maximum segments per one DMA request */
	if (max_segments[hwif->major] == NULL) {
	    int minors = MAX_DRIVES * (1 << PARTN_BITS);
	    dma_max_segments = kmalloc(minors * sizeof(int), GFP_KERNEL);
	    if (dma_max_segments) {
		for (i = 0; i < minors; i++)
		    dma_max_segments[i] = DMA_MAX_SEGMENTS;
		max_segments[hwif->major] = dma_max_segments;
	    }
	}

	printk("\n");
	return;

error:
	printk(" -- cannot bind to DMA relay module\n");
}
