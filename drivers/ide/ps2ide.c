#include <linux/config.h>
#include <linux/types.h>
#include <linux/ide.h>
#include <linux/delay.h>
#include <asm/ide.h>
#include <asm/hdreg.h>
#include <asm/bootinfo.h>
#include <asm/ps2/irq.h>

#include "ide_modes.h"

#ifdef DEBUG
#define DPRINT(fmt, args...) \
	do { \
		printk("ps2ide: " fmt, ## args); \
	} while (0)
#define DPRINTK(fmt, args...) \
	do { \
		printk(fmt, ## args); \
	} while (0)
#else
#define DPRINT(fmt, args...) do {} while (0)
#define DPRINTK(fmt, args...) do {} while (0)
#endif

extern int ps2_pccard_present;

#define AIF_HDD_TCFG	0xb8000040
#define AIF_HDD_BASE	0xb8000060
#define PS2_HDD_BASE	0xb4000040

static int ps2_ide_offsets[IDE_NR_PORTS] __initdata = {
	0x00, 0x02, 0x04, 0x06, 0x08, 0x0a, 0x0c, 0x0e, 0x1c, -1
};

static void ps2_ide_select(ide_drive_t *drive)
{
	int retry;
	ide_hwif_t *hwif = HWIF(drive);

	if (drive->drive_data || drive->select.b.unit)
		return;

	DPRINT("waiting for spinup... 0x%x\n", hwif->io_ports[0]);
	for (retry = 0; retry < 160; retry++) { /* 50msec X 160 = 8sec */
		/* write select command */
		OUT_BYTE(drive->select.all, hwif->io_ports[IDE_SELECT_OFFSET]);

		/* see if we've select successfully */
		if (IN_BYTE(hwif->io_ports[IDE_SELECT_OFFSET]) ==
		    drive->select.all) {
			DPRINTK(": OK\n");
			break;
		}
		/* wait 50msec */
		mdelay(50);
	}
	DPRINTK("\n");
	drive->drive_data = 1;
}

#ifdef CONFIG_T10000_AIFHDD
static void ps2_aif_ide_tune_drive(ide_drive_t *drive, byte mode_wanted)
{
	ide_pio_data_t d;
	int mode = 0;
	int unit = -1;
	int i;

	for (i = 0; i < MAX_DRIVES; i++) {
		if (&HWIF(drive)->drives[i] == drive) {
			unit = i;
			break;
		}
	}

	ide_get_best_pio_mode(drive, mode_wanted, 4, &d);
	mode = (d.use_iordy << 3) | d.pio_mode;
	printk("%s: AIF tune: unit%d, mode=%d\n", drive->name, unit, d.pio_mode);
	switch (unit) {
	case 0:
		outb((inb(AIF_HDD_TCFG) & 0xf0) + mode, AIF_HDD_TCFG);
		break;
	case 1:
		outb((inb(AIF_HDD_TCFG) & 0x0f) + (mode << 4), AIF_HDD_TCFG);
		break;
	default:
		break;
	}
}
#endif

static int ps2_ide_default_irq(ide_ioreg_t base)
{
	return 0;
}

static ide_ioreg_t ps2_ide_default_io_base(int index)
{
	return 0;
}

static void ps2_ide_init_hwif_ports (hw_regs_t *hw, ide_ioreg_t data_port,
                                     ide_ioreg_t ctrl_port, int *irq)
{
}

static int ps2_ide_request_irq(unsigned int irq,
                                void (*handler)(int,void *, struct pt_regs *),
                                unsigned long flags, const char *device,
                                void *dev_id)
{
	return request_irq(irq, handler, flags|SA_SHIRQ, device, dev_id);
}			

static void ps2_ide_free_irq(unsigned int irq, void *dev_id)
{
	free_irq(irq, dev_id);
}

static int ps2_ide_check_region(ide_ioreg_t from, unsigned int extent)
{
	return check_region(from, extent);
}

static void ps2_ide_request_region(ide_ioreg_t from, unsigned int extent,
                                    const char *name)
{
	request_region(from, extent, name);
}

static void ps2_ide_release_region(ide_ioreg_t from, unsigned int extent)
{
	release_region(from, extent);
}

struct ide_ops ps2_ide_ops = {
	&ps2_ide_default_irq,
	&ps2_ide_default_io_base,
	&ps2_ide_init_hwif_ports,
	&ps2_ide_request_irq,
	&ps2_ide_free_irq,
	&ps2_ide_check_region,
	&ps2_ide_request_region,
	&ps2_ide_release_region
};

#ifdef CONFIG_T10000_AIFHDD
void __init ps2_aif_register(void)
{
	hw_regs_t hw;
	ide_hwif_t *hwif;
	int index;
	int i;

	ide_setup_ports(&hw, (ide_ioreg_t)AIF_HDD_BASE, ps2_ide_offsets,
			0, 0, NULL, IRQ_SBUS_AIF);
	index = ide_register_hw(&hw, NULL);

	hwif = &ide_hwifs[index];
	hwif->tuneproc = ps2_aif_ide_tune_drive;
	for (i = 0; i < MAX_DRIVES; i++)
		hwif->drives[i].autotune = 1;
}
#endif

#ifdef CONFIG_BLK_DEV_PS2_IDEDMA
void ps2_ide_setup_dma(ide_hwif_t *hwif);
#endif

void __init ps2_hdd_register(void)
{
	hw_regs_t hw;
	ide_hwif_t *hwif;
	int index;

	ide_setup_ports(&hw, (ide_ioreg_t)PS2_HDD_BASE, ps2_ide_offsets,
			0, 0, NULL, IRQ_SBUS_PCIC);
	index = ide_register_hw(&hw, NULL);

	hwif = &ide_hwifs[index];
	hwif->selectproc = ps2_ide_select;
#ifdef CONFIG_BLK_DEV_PS2_IDEDMA
	ps2_ide_setup_dma(hwif);
#endif
}

void __init ps2_ide_init(void)
{
#ifdef CONFIG_T10000_AIFHDD
	if (mips_machtype == MACH_T10000)
		ps2_aif_register();
#endif

	if (ps2_pccard_present == 0x0100)
		ps2_hdd_register();
}
