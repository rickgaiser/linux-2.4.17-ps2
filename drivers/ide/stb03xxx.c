/*
 *    Copyright 2001 MontaVista Software Inc.
 *      Completed implementation.
 *      Author: MontaVista Software, Inc.  <source@mvista.com>
 *		Hai-Pao Fan <hpfan@mvista.com>
 *
 *    Module name: stb03xxx.c
 *
 *    Description:
 */

#include <linux/types.h>
#include <linux/hdreg.h>
#include <linux/delay.h>
#include <linux/ide.h>

#include <asm/io.h>
#include <asm/scatterlist.h>
#include <asm/ppc405_dma.h>

#include "ide_modes.h"

#define REDWOOD_IDE_CMD		0xf2100000
#define REDWOOD_IDE_CTRL	0xf4100000
#define IDE_CMD_OFF		0x00100000
#define IDE_CTL_OFF		0x00100000

#define IDE_INTRQ 	25		/* interrupt level(int0) */

#define REDWOOD_DMA_ADDR	0xfce00000


/* use DMA channel 2 for IDE DMA operations */

#define IDE_DMA_INT	6		/* IDE dma channel 2 interrupt */

/* DMACR2 */
#define IDE_DMACR_CE	0x80000000	/* 00 channel enable */
#define IDE_DMACR_CIE	0x40000000	/* 01 channel interrupt enable */
#define IDE_DMACR_TD	0x20000000	/* 02 transfer direction */
					/* 0 = write 1 = read */
#define IDE_DMACR_PL	0x10000000	/* 03 peripheral location */
#define IDE_DMACR_PW	0x0C000000	/* 04-05 peripheral width */
#define IDE_DMACR_DAI	0x02000000	/* 06 destination address increment */
#define IDE_DMACR_SAI	0x01000000	/* 07 source address increment */
#define IDE_DMACR_CP	0x00800000	/* 08 high order channel priority bit*/
#define IDE_DMACR_TM	0x00600000	/* 09-10 transfer mode */
#define IDE_DMACR_PSC	0x00180000	/* 11-12 peripheral setup cycles */
#define IDE_DMACR_PWC	0x0007E000	/* 13-18 peripheral wait cycles */
#define IDE_DMACR_PHC	0x00001C00	/* 19-21 peripheral hold cycles */
#define IDE_DMACR_ETD	0x00000200	/* 22 EOT/TC */
#define IDE_DMACR_TCE	0x00000100	/* 23 TC Enable */
#define IDE_DMACR_CH	0x00000080	/* 24 chaining enable */
#define IDE_DMACR_ECE	0x00000020	/* 26 EOT chain mode enable */
#define IDE_DMACR_TCD	0x00000010	/* 27 TC chain mode enable */
#define IDE_DMACR_DEC	0x00000004	/* 29 destination address decrement */
#define IDE_DMACR_CP1	0x00000001	/* 31 low order channel priority bit */

#define IDE_DMASR_TC	0x20000000
#define IDE_DMASR_EOT	0x02000000
#define IDE_DMASR_ERR	0x00200000
#define IDE_DMASR_CB	0x00000100
#define IDE_DMASR_CT	0x00000020

unsigned long dmacr_def = 0x0000AB02;	/* pwc=101 phc=10, res:30=1 */
extern char *ide_dmafunc_verbose(ide_dma_action_t dmafunc);


#define WMODE	0		/* default to DMA line mode */
#define PIOMODE	0
static volatile unsigned long dmastat;

/* Function Prototypes */
static void redwood_ide_tune_drive(ide_drive_t *, byte);
static byte redwood_ide_dma_2_pio(byte);
static int redwood_ide_tune_chipset(ide_drive_t *, byte);
static int redwood_ide_dmaproc(ide_dma_action_t, ide_drive_t *);


static void redwood_ide_tune_drive(ide_drive_t *drive, byte pio)
{
	pio = ide_get_best_pio_mode(drive, pio, 5, NULL);
}

static byte redwood_ide_dma_2_pio(byte xfer_rate)
{
	switch(xfer_rate) {
	case XFER_UDMA_5:
	case XFER_UDMA_4:
	case XFER_UDMA_3:
	case XFER_UDMA_2:
	case XFER_UDMA_1:
	case XFER_UDMA_0:
	case XFER_MW_DMA_2:
	case XFER_PIO_4:
		return 4;
	case XFER_MW_DMA_1:
	case XFER_PIO_3:
		return 3;
	case XFER_SW_DMA_2:
	case XFER_PIO_2:
		return 2;
	case XFER_MW_DMA_0:
	case XFER_SW_DMA_1:
	case XFER_SW_DMA_0:
	case XFER_PIO_1:
	case XFER_PIO_0:
	case XFER_PIO_SLOW:
	default:
		return 0;
	}
}

static int redwood_ide_tune_chipset (ide_drive_t *drive, byte speed)
{
	int err=0;

	redwood_ide_tune_drive(drive, redwood_ide_dma_2_pio(speed));

	if (!drive->init_speed)
		drive->init_speed = speed;
	err = ide_config_drive_speed(drive, speed);
	drive->current_speed = speed;
	return err;	
}

static int redwood_config_drive_for_dma(ide_drive_t *drive)
{
	struct hd_driveid *id = drive->id;
	byte speed;
	int func = ide_dma_off;

	/*
	 * Enable DMA on any drive that has multiword DMA
	 */
	if (id->field_valid & 2) {
		if (id->dma_mword & 0x0004) {
			speed = XFER_MW_DMA_2;
			func = ide_dma_on;
		} else if (id->dma_mword & 0x0002) {
			speed = XFER_MW_DMA_1;
			func = ide_dma_on;
		} else if (id->dma_mword & 1) {
			speed = XFER_MW_DMA_0;
			func = ide_dma_on;
		} else if (id->dma_1word & 0x0004) {
			speed = XFER_SW_DMA_2;
			func = ide_dma_on;
		} else {
			speed = XFER_PIO_0 + 
				ide_get_best_pio_mode(drive, 255, 5, NULL);
		}
	}

	redwood_ide_tune_drive(drive, redwood_ide_dma_2_pio(speed));
	return redwood_ide_dmaproc(func, drive);
}

ide_startstop_t redwood_ide_intr (ide_drive_t *drive)
{
	int i;
	byte dma_stat;
	unsigned int nsect;
	ide_hwgroup_t *hwgroup = HWGROUP(drive);
	struct request *rq = hwgroup->rq;
	unsigned long block,b1,b2,b3,b4;

	nsect = rq->current_nr_sectors;

	dma_stat = HWIF(drive)->dmaproc(ide_dma_end, drive);

	rq->sector += nsect;
	rq->buffer += nsect<<9;
	rq->errors = 0;
	i = (rq->nr_sectors -= nsect);
	ide_end_request(1, HWGROUP(drive));
	if (i > 0) {
		b1 = IN_BYTE(IDE_SECTOR_REG);
		b2 = IN_BYTE(IDE_LCYL_REG);
		b3 = IN_BYTE(IDE_HCYL_REG);
		b4 = IN_BYTE(IDE_SELECT_REG);
		block = ((b4 & 0x0f) << 24) + (b3 << 16) + (b2 << 8) + (b1);
		block++;
		if (drive->select.b.lba) {
			OUT_BYTE(block,IDE_SECTOR_REG);
			OUT_BYTE(block>>=8,IDE_LCYL_REG);
			OUT_BYTE(block>>=8,IDE_HCYL_REG);
			OUT_BYTE(((block>>8)&0x0f)|drive->select.all,IDE_SELECT_REG);
		} else {
			unsigned int sect,head,cyl,track;
			track = block / drive->sect;
			sect = block % drive->sect + 1;
			OUT_BYTE(sect,IDE_SECTOR_REG);
			head = track % drive->head;
			cyl = track / drive->head;
			OUT_BYTE(cyl,IDE_LCYL_REG);
			OUT_BYTE(cyl>>8,IDE_HCYL_REG);
			OUT_BYTE(head|drive->select.all,IDE_SELECT_REG);
		}

		if (rq->cmd == READ)
			dma_stat = HWIF(drive)->dmaproc(ide_dma_read, drive);
		else
			dma_stat = HWIF(drive)->dmaproc(ide_dma_write, drive);
		return ide_started;
	}
	return ide_stopped;
}

void redwood_ide_dma_intr (int irq, void *dev_id, struct pt_regs *regs)
{
	dmastat = mfdcr(DCRN_DMASR);

	mtdcr(DCRN_DMASR,
		IDE_DMASR_TC | IDE_DMASR_EOT | IDE_DMASR_ERR | IDE_DMASR_CT);

	/* clear UIC bit */
	mtdcr(DCRN_UIC0_SR, (0x80000000 >> IDE_DMA_INT));
}

static int redwood_ide_dmaproc(ide_dma_action_t func, ide_drive_t *drive)
{
	ide_hwif_t *hwif = HWIF(drive);
	int i, reading = 0;
	struct request *rq=HWGROUP(drive)->rq;
	unsigned long flags;
	unsigned long length;

	switch (func) {
	case ide_dma_off:
	case ide_dma_off_quietly:
		/*disable_dma*/
		return 0;

	case ide_dma_on:
#if PIOMODE
		return 1;
#endif

		mtdcr(DCRN_DMACR2,0);
		mtdcr(DCRN_DMASR, IDE_DMASR_TC | IDE_DMASR_EOT | IDE_DMASR_ERR
				| IDE_DMASR_CT);

		/* level-sensitive (UICTR bit = 0) and positive-active (UICPR bit = 1) */
		mtdcr(DCRN_UIC0_PR, (mfdcr(DCRN_UIC0_PR) | UIC_D2));
		save_flags(flags);
		cli();
		if ( ide_request_irq(IDE_DMA_INT, &redwood_ide_dma_intr, SA_INTERRUPT,
			hwif->name, hwif->hwgroup) ) {
			printk("ide_redwood: ide_request_irq failed int=%d\n",
					IDE_DMA_INT);
			restore_flags(flags);
			return 1;
		}
		restore_flags(flags);

		drive->using_dma = (func == ide_dma_on);
#if WMODE
		mtdcr(DCRN_DCRXBCR, 0);
		mtdcr(DCRN_CICCR, mfdcr(DCRN_CICCR) | 0x00000400);
#else
		/* Configure CIC reg for line mode dma */
		mtdcr(DCRN_CICCR, mfdcr(DCRN_CICCR) & ~0x00000400);
#endif
		return 0;	

	case ide_dma_check:
		return redwood_config_drive_for_dma(drive);
	case ide_dma_read:
		reading = 1;
	case ide_dma_write:
		if (drive->media != ide_disk)
			return -1;

		if (mfdcr(DCRN_DMACR2) & IDE_DMACR_CE)	/* DMA is busy? */
			return -1;

		mtdcr(DCRN_DMASR, IDE_DMASR_TC | IDE_DMASR_EOT | IDE_DMASR_ERR
				| IDE_DMASR_CT);

		if (reading) {
			dma_cache_inv((unsigned long) rq->buffer,
				rq->current_nr_sectors * 512);
#if WMODE
			mtdcr(DCRN_DMASA2,0);
#else
			mtdcr(DCRN_DMASA2,0xfce00000);
#endif
			mtdcr(DCRN_DMADA2, virt_to_bus(rq->buffer));
		} else {
			dma_cache_wback_inv((unsigned long) rq->buffer,
				rq->current_nr_sectors * 512);
			mtdcr(DCRN_DMASA2, virt_to_bus(rq->buffer));
#if WMODE
			mtdcr(DCRN_DMADA2,0);
#else
			mtdcr(DCRN_DMADA2,0xfce00000);
#endif
		}

#if WMODE
		length = rq->current_nr_sectors * 512/2;
#else
		length = rq->current_nr_sectors * 512/16;
#endif
		OUT_BYTE(rq->current_nr_sectors,IDE_NSECTOR_REG);
		mtdcr(DCRN_DMACT2, length);
		
		/* CE=0 disable DMA */
		/* Set up the Line Buffer Control Register
		 * 11d1xxxx0.. - 11=Mode 2 (120 ns cycle), d=1/0(read/write)
		 * 1=active, 0=1X clock mode.
		 */

		if (reading) {
#if WMODE
			mtdcr(DCRN_DMACR2, 0x66000000 | dmacr_def);
#else
			mtdcr(DCRN_DCRXBCR,0xD0000000);
			mtdcr(DCRN_DMACR2, 0x6E600000 | dmacr_def);
#endif
		} else {
#if WMODE
			mtdcr(DCRN_DMACR2, 0x46000000 | dmacr_def);
#else
			mtdcr(DCRN_DCRXBCR, 0xF0000000);
			mtdcr(DCRN_DMACR2, 0x4D600000 | dmacr_def);
#endif
		}

		set_dma_mode(hwif->hw.dma, reading
				? DMA_MODE_READ : DMA_MODE_WRITE);
		drive->waiting_for_dma = 1;
		ide_set_handler(drive, &redwood_ide_intr, WAIT_CMD, NULL);
		OUT_BYTE(reading ? WIN_READDMA : WIN_WRITEDMA, IDE_COMMAND_REG);

	case ide_dma_begin:
		/* enable DMA */
		mtdcr(DCRN_DMACR2,mfdcr(DCRN_DMACR2) | 0x80000000);

		/* wait for dma to complete (channel 2 terminal count) */
		for (i=0; i < 5000000; i++) {
			if (dmastat & DMA_CS2)
				break;	
		}
		dmastat = 0;
		return 0;

	case ide_dma_end:
		drive->waiting_for_dma = 0;

		/* disable DMA */
		mtdcr(DCRN_DMACR2, mfdcr(DCRN_DMACR2) & ~0x80000000);
		mtdcr(DCRN_DCRXBCR,0);
		return 0;

	case ide_dma_test_irq:
		return 1;	/* returns 1 if dma irq issued, 0 otherwise */

	case ide_dma_bad_drive:
	case ide_dma_good_drive:
	//	return check_drive_lists(drive, (func == ide_dma_good_drive));
	case ide_dma_verbose:
	//	return report_drive_dmaing(drive);
	case ide_dma_timeout:
	case ide_dma_retune:
	case ide_dma_lostirq:
		printk("ide_dmaproc: chipset supported %s func only: %d\n",
				ide_dmafunc_verbose(func), func);
		return 1;
	default:
		printk("ide_dmaproc: unsupported %s func: %d\n", ide_dmafunc_verbose(func), func);
			return 1;

		//return ide_dmaproc(func, drive);
	}

}

int nonpci_ide_default_irq(ide_ioreg_t base)
{
	return IDE_INTRQ;
}

void
nonpci_ide_init_hwif_ports(hw_regs_t *hw, ide_ioreg_t data_port, ide_ioreg_t ctrl_port, int *irq)
{
	unsigned long reg = data_port;
	unsigned long ioaddr;
	unsigned long xilinx;
	int i, index;

	if (!request_region(REDWOOD_IDE_CMD, 0x10, "IDE"))
		return;

	if (!request_region(REDWOOD_IDE_CTRL, 2, "IDE")) {
		release_region(REDWOOD_IDE_CMD, 0x10);
		return;
	}

	mtdcr(DCRN_DCRXICR, 0x40000000);	/* set dcrx internal arbiter */

	/* reconstruct phys addrs from EBIU config regs for CS2# */
	reg = ((mfdcr(DCRN_BRCR2) & 0xff000000) >> 4) | 0xf0000000;
	xilinx = reg | 0x00040000;
	reg = reg | IDE_CMD_OFF;

#if 0
	printk("orig xilinx %08x, reg %08x\n", xilinx, reg);
	reg = 0xf2100000;
	xilinx = 0xf2040000;
	printk("before ioremap ioaddr %x, xilinx %x\n", reg, xilinx);
#endif

	ioaddr = (unsigned long)ioremap(reg, 0x10);
	xilinx = (unsigned long)ioremap(xilinx, 0x10);

	hw->irq = IDE_INTRQ;

	for (i = IDE_DATA_OFFSET; i <= IDE_STATUS_OFFSET; i++) {
		hw->io_ports[i] = ioaddr;
		ioaddr += 2;
	}
	hw->io_ports[IDE_CONTROL_OFFSET] = (unsigned long)ioremap(REDWOOD_IDE_CTRL,2);

#if 0
	/* ftr revisit
	** Is this needed?  If so, shouldn't just slam a value in, should
	** read existing value and just modify the bit we care about.
	** Note that this impacts other devices, such as ethernet
	*/
	mtdcr(DCRN_BRCR2, 0x207cfffe);	/* RE=1 */
#endif

	/* add RE & OEN to value set by boot rom */
	mtdcr(DCRN_BRCR3, 0x407cfffe);

	/* init CIC control reg to enable IDE interface PIO mode */
	mtdcr(DCRN_CICCR, (mfdcr(DCRN_CICCR) & 0xffff7bff) | 0x0003);

	i=readw(xilinx);
	if(i & 0x0001) {
		writew( i & ~0x8001, xilinx);
		writew( 0, xilinx+7*2);
		udelay(10*1000);	/* 10 ms */
	}

	/* init xilinx control registers - enable ide mux, clear reset bit */
	writew( i | 0x8001, xilinx);
	writew( 0, xilinx+7*2);

	/* wait until drive is not busy (it may be spinning up) */
	for (i=0; i < 10; i++) {
		unsigned char stat;
		stat = inb_p(hw->io_ports[7]);
		/* wait for !busy & ready */
		if ((stat & 0x80) == 0)
			break;

		udelay(1000*1000);		/* 1 second */
	}

	/* use DMA channel 2 for IDE DMA operations */
	hw->dma = 2;

	mtdcr(DCRN_DMACR2, 0x4d600000 | dmacr_def);
	mtdcr(DCRN_DMASR, 0xffffffff); /* clear status register */

	/* init CIC select2 reg to connect external DMA port 3 to internal 
	 * DMA channel 2
	 */
	mtdcr(DCRN_DMAS2, (mfdcr(DCRN_DMAS2) & 0xfffffff0) | 0x00000002);

	index = 0;

	ide_hwifs[index].tuneproc = &redwood_ide_tune_drive;
	ide_hwifs[index].drives[0].autotune = 1;
/*
	ide_hwifs[index].drives[1].autotune = 1;
*/

	ide_hwifs[index].autodma = 1;
	ide_hwifs[index].dmaproc = &redwood_ide_dmaproc;
	ide_hwifs[index].speedproc = &redwood_ide_tune_chipset;

	ide_hwifs[index].noprobe = 0;

	memcpy(ide_hwifs[index].io_ports, hw->io_ports, sizeof(hw->io_ports));
	ide_hwifs[index].irq = IDE_INTRQ;
}

