/*
 *  drivers/vme/universe.c -- Driver for Tundra Universe PCI<->VME bridge.
 *
 *  Copyright (C) 1997-1999 Gabriel Paubert, paubert@iram.es
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License.  See the file COPYING in the main directory of this archive
 *  for more details.
 */

/* Restrictions: 
 * - only one Universe supported per system (but is it truly a restriction ?
 *  The theories about multiple Universes are still extremely speculative 
 *  and cannot be relied upon, so they are best avoided altogether :-)) 
 * - currently only works on PPC processors, although every attempt has
 *  been made to make porting to other architectures relatively easy. 
 * - this driver has been designed for VME crates in which there is
 *  basically only one master and all other boards are slaves. 
 * - the Universe is a very complex chip and it had a significant number of
 *  errata, which are published in a document by Tundra. In simple 
 *  environments (the Universe is the only master on the VME bus), most
 *  of these bugs can be worked around by avoiding posted writes and
 *  by setting bursts of 32 bytes.
 */

/* Known problems: 
 * -DMA timeout handler is not guaranteed to succesfully terminate pending 
 * DMA operations since it seems that the Universe may require owning the 
 * VMEbus before acknowledging a DMA STOP request. So if the bus is taken 
 * permanently by another master, there may be no way of recovering from this 
 * condition (note that this will never be a problem when the Universe is the 
 * only possible master, and the code will detect starvation due to excessive 
 * use of the coupled and posted writes channels).
 * 
 * - The handling of unexpected interrupts requires more thought. Current
 * implementation is fairly safe and should work properly in most cases even
 * when designing and debugging hardware, but requires unloading and reloading
 * the driver when a problem happens and a level becomes permanently disabled.
 */


/* Undefine this if your BIOS/Firmware allows you to select the universe
 * slave image configuration. Otherwise edit the code which is enabled by
 * this macro to suit your needs.
 */

#define UNIVERSE_IMAGE_SETUP

/* On some boards  triggering the VME reset is suicide since it reboots */
#define SUICIDAL_VME_RESET

#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#ifdef __powerpc__
#include <asm/pgtable.h>
#endif

#include "universe.h"

EXPORT_SYMBOL(vme_register_device);
EXPORT_SYMBOL(vme_unregister_device);

EXPORT_SYMBOL(vme_register_region);
EXPORT_SYMBOL(vme_unregister_region);

EXPORT_SYMBOL(vme_request_interrupt);
EXPORT_SYMBOL(vme_free_interrupt);

EXPORT_SYMBOL(vme_alloc_dmalist);
EXPORT_SYMBOL(vme_queue_dmalist);
EXPORT_SYMBOL(vme_free_dmalist);

EXPORT_SYMBOL(vme_safe_access);
EXPORT_SYMBOL(vme_modbits);

/* Module parameters: 
 * a) a bit mask to tell which images must be permanently mapped. Permanent 
 * maps are handy in some cases, and are the only ones on which the read() 
 * and write() system calls work, but may lead to large kernel virtual memory 
 * space consumption. All other images are dynamically mapped in kernel space 
 * as required by the board specific driver or in user space by the mmap 
 * system call.
 * b) a flag to reset the VME bus at module initialization, but only if
 * SUICIDAL_VME_RESET is not defined. 
 */

static u_int permanent_maps = 0x03;
MODULE_PARM(permanent_maps, "i");
#ifndef SUICIDAL_VME_RESET
static u_int reset = 0;
MODULE_PARM(reset, "i");
#endif

/* This has been directly copied from drivers/char/mem.c, as the comment
 * say it is ugly to have to repeat it here. 
 */

/*
 * This should probably be per-architecture in <asm/pgtable.h>
 */
static inline 
unsigned long pgprot_noncached(unsigned long prot) {
#if defined(__i386__)
	if (boot_cpu_data.x86 > 3)
		prot |= _PAGE_PCD;
#elif defined(__powerpc__)
	prot |= _PAGE_NO_CACHE | _PAGE_GUARDED;
#elif defined(__mc68000__)
	if (CPU_IS_020_OR_030)
		prot |= _PAGE_NOCACHE030;
	/* Use no-cache mode, serialized */
	if (CPU_IS_040_OR_060)
		prot = (prot & _CACHEMASK040) | _PAGE_NOCACHE_S;
#elif defined(__mips__)
	prot = (prot & ~_CACHE_MASK) | _CACHE_UNCACHED;
#endif

	return prot;
}

/* End of drivers/char/mem.c stolen code. */

#ifndef __powerpc__
#define iobarrier_rw() do { } while(0)
#define iobarrier_w() do { } while(0)
#endif

/* Defining this makes the initialization verbose, perhaps too verbose ! */
#undef UNIVERSE_DEBUG


/* bit indexes for the state field in the private section */
#define IRQ_ACTIVE   0
#define DMA_ACTIVE   1
#define DMA_CANCEL   2	/* Not used ATM */
#define DMA_TIMEDOUT 3
#define IRQ_ACTIVE_MASK (1<<IRQ_ACTIVE)

static loff_t universe_llseek(struct file *, loff_t, int);
static ssize_t universe_read(struct file *, char *, size_t, loff_t *);
static ssize_t universe_write(struct file *, const char *, size_t, loff_t *);
static int universe_open(struct inode *, struct file *);
static int universe_release(struct inode *, struct file *);
static int universe_ioctl(struct inode *, struct file *, 
			  unsigned int, unsigned long);
static int universe_mmap(struct file *, struct vm_area_struct *); 

static void dma_timeout(unsigned long);

static struct file_operations universe_fops = { 
	owner:     NULL,
	llseek:    universe_llseek,
	read:      universe_read,
	write:     universe_write,
	ioctl:     universe_ioctl,
	mmap:      universe_mmap,
	open:      universe_open,
	release:   universe_release,
};

static struct vme_device vme_root={fops: &universe_fops, 
				   name: "vme (Universe)", 
				   minor: 0};

/* Note that locks have been spaced in this region to avoid playing ping-pong
 * with cache lines when several processors are trying to acquire different
 * locks.
 */

static struct private {
	rwlock_t lock;		      /* For general structures */
  	struct pci_dev *pci;	      /* Pointer to device in PCI data base */
  	volatile u_char * reg_base;   /* To access internal registers */
	u_long bus_delta;
	struct vme_region root_regions[16];
  	/* Variables used by DMA are grouped here*/ 
	spinlock_t dma_lock;
	struct vme_dma * dma_queue;   /* pending DMA operations */
  	struct vme_dma * dma_queue_end; /* last queued DMA operation */
  	volatile int state;           /* Most state changes require dma_lock */
	struct timer_list dma_timer;
  	u32 cached_dgcs;	      /* to minimize reads of this reg */
	wait_queue_head_t dma_free_wait;
        spinlock_t bus_lock;	      /* For RMW and safe accesses */
  	unsigned char revision;	      /* Universe revision level */

  	/* Variables used for interrupt handling */ 
	u32 cached_lint_en;	      /* to minimize reads of this reg */
  	u32 valid_virqs;	      /* Permanently disabled VME ints */
  	struct {
	  	u_long errors;        /* Number of errors on this level */
	  	u_long unhandled;     /* Number of unhandled interrupts */
	  	u_long tstamp;        /* Time stamp of last error */
  		u_int handlers;       /* Number of handlers on this level */
	  	u_int vector;         /* Vector of last error */
	} virq[7];
  	struct vme_interrupt *ints[7][256];
} universe = {
	root_regions: { /* Regular slave images 0 to 7 */
	  		[0 ... 7] = {device: &vme_root},
			/* Special slave image, part 0 */
  			{device: &vme_root,
			 base: 0,
			 limit: 0x00feffff},
			/* Special slave image, part 1 */
  		        {device: &vme_root,
			 base: 0,
			 limit: 0x0000ffff},
			/* Special slave image, part 2 */
  			{device: &vme_root,
			 base: 0,
			 limit: 0x00feffff},
			/* Special slave image, part 3 */
  		        {device: &vme_root,
			 base: 0,
			 limit: 0x0000ffff},
			/* Special slave image, part 4 */
  			{device: &vme_root,
			 base: 0,
			 limit: 0x00feffff},
			/* Special slave image, part 5 */
  		        {device: &vme_root,
			 base: 0,
			 limit: 0x0000ffff},
			/* Special slave image, part 6 */
  			{device: &vme_root,
			 base: 0,
			 limit: 0x00feffff},
			/* Special slave image, part 7 */
  		        {device: &vme_root,
			 base: 0,
			 limit: 0x0000ffff},
	},
	bus_delta: 0,
	dma_timer: {function:  dma_timeout,
		    data:      (u_long) &universe},
	cached_lint_en: UNIVERSE_LINT_DMA,
	valid_virqs: UNIVERSE_LINT_VIRQS,
};

/* The following tables are not declared const because they might need
 * to be modified at runtime if user AM codes support is added.
 */

/* Table to convert VME AM codes to dctl register values, the bridge
 * then will transform them the other way around. Note that we have to set
 * VDW mask since the actual flags are built by and'ing this entry with
 * dw2ctl[] entry.
 */
static u32 am2ctl[]={
#define EN(am) (UNIVERSE_SLAVE_EN | UNIVERSE_VAS_##am | UNIVERSE_VDW_MASK)
	/* A64 not supported */
  	[0x00 ... 0x07] = 0,
	EN(A32) | UNIVERSE_BLT,
	EN(A32),
	EN(A32) | UNIVERSE_SPACE_PROGRAM,
        EN(A32) | UNIVERSE_BLT,
	EN(A32) | UNIVERSE_PRIV_SUPER | UNIVERSE_BLT,
	EN(A32) | UNIVERSE_PRIV_SUPER,
	EN(A32) | UNIVERSE_PRIV_SUPER | UNIVERSE_SPACE_PROGRAM,
	EN(A32) | UNIVERSE_PRIV_SUPER | UNIVERSE_BLT,
	/* User reserved AM not supported */
	[0x10 ... 0x1f] = 0, 
	[0x20 ... 0x27] = 0,
	0, EN(A16), 0, 0,
	0, EN(A16) | UNIVERSE_PRIV_SUPER, 0, EN(CRCSR),
	[0x30 ... 0x37] = 0,
	EN(A24) | UNIVERSE_BLT,
	EN(A24),
	EN(A24) | UNIVERSE_SPACE_PROGRAM,
        EN(A24) | UNIVERSE_BLT,
	EN(A24) | UNIVERSE_PRIV_SUPER | UNIVERSE_BLT,
	EN(A24) | UNIVERSE_PRIV_SUPER,
	EN(A24) | UNIVERSE_PRIV_SUPER | UNIVERSE_SPACE_PROGRAM,
	EN(A24) | UNIVERSE_PRIV_SUPER | UNIVERSE_BLT,
#undef EN
};	

/*
 * Some address spaces do not support all types of accesses. 
 */

static u32 am_bad_use[]=
{	[0x00 ... 0x07] = VME_USE_MASK,
	[0x08 ... 0x0f] = 0,
	/* User reserved AM not supported */
	[0x10 ... 0x1f] = VME_USE_MASK,
	[0x20 ... 0x27] = VME_USE_MASK,
	VME_USE_MASK, 0, VME_USE_MASK, VME_USE_MASK,
	VME_USE_MASK, 0, VME_USE_MASK, VME_USE_DMA | VME_USE_RMW,
	[0x30 ... 0x37] = VME_USE_MASK,
	[0x38 ... 0x3f] = 0,
};

/* 
 * No address space supports all possible bus width settings. 
 */
static u32 am_bad_dw[] = {
	[0x00 ... 0x07] = VME_DW_MASK,	/* A64 not supported */
	VME_DW(8|16|32), 
	[0x09 ... 0x0b] = VME_DW(64),
	VME_DW(8|16|32), 
	[0x0d ... 0x0f] = VME_DW(64),
	/* User reserved AM not supported */
	[0x10 ... 0x1f] = VME_DW_MASK,
	[0x20 ... 0x27] = VME_DW_MASK,
	VME_DW_MASK, VME_DW(64), VME_DW_MASK, VME_DW_MASK,
	VME_DW_MASK, VME_DW(64), VME_DW_MASK, VME_DW(64),
	[0x30 ... 0x37] = VME_DW_MASK,
	VME_DW(8|16|32),
	[0x39 ... 0x3b] = VME_DW(64),
	VME_DW(8|16|32),
	[0x3d ... 0x3f] = VME_DW(64),
};

/* Table to convert bus width to Universe DCTL bits: used by DMA and 
 * ioctl accesses, this value is actually anded with the value from the 
 * am2ctl table, so invalid values are filled with zeroes to reset
 * the UNIVERSE_SLAVE_EN bit.
 */
static const u32 
dw2ctl[]={0, UNIVERSE_VDW_8|~UNIVERSE_VDW_MASK, 
	  UNIVERSE_VDW_16|~UNIVERSE_VDW_MASK, 0, 
	  UNIVERSE_VDW_32|~UNIVERSE_VDW_MASK, 0, 0, 0, 
	  UNIVERSE_VDW_64|~UNIVERSE_VDW_MASK, [9 ... 15]= 0};

/* Table used to convert SLSI flags to VME AM, the SLSI has 3 flags per
 * 16 Mb section, but one is ignored for the A16 subsection and the VDW
 * parameter is completely independant. It's easier to define it as a 3 
 * dimensional array.
 */
static const __initdata u32 
slsi2am[2/*A24/A16*/][2/*Data/Program*/][2/*User/Super*/] =
{
	{
	  	{VME_AM_A24(0),      VME_AM_A24_PRIV(0)},
		{VME_AM_A24_PROG(0), VME_AM_A24_PRIV_PROG(0)}
	}, { 
		{VME_AM_A16(0),      VME_AM_A16_PRIV(0)},
		{VME_AM_A16(0),      VME_AM_A16_PRIV(0)}
	}
};


/* Table (somewhat verbose) used to convert Universe slave image control 
 * registers to VME root regions flags: the order in this table is important 
 * since it's scanned from the beginning (at most 8 times during driver 
 * initialization so it's not performance critical).
 * 
 * The Universe documentation is somewhat contradictory: it claims that 
 * setting the slave control registers to illegal combinations may result
 * in invalid VME cycles, but that it will only consider the setting
 * of the BLT bit for A24 and A32 spaces (but that this bit will be overriden
 * by setting VDW=64 which allows MBLT in these spaces). The following
 * is somewhat more strict, it considers a programming error the following 
 * cases: 
 * - setting VDW=64 but BLT off,
 * - setting BLT or VDW=64 for A16 and CR/CSR spaces,
 * this is in any case quite easy to change. 
 */
static const __initdata struct ctl2am_t {
	u32 mask, value, flags;
} ctl2am[] = {
  	{UNIVERSE_VDW_MASK  | UNIVERSE_BLT_MASK,
	 UNIVERSE_VDW_64    | UNIVERSE_NOBLT,
	 ~0U},
	{UNIVERSE_VAS_MASK  | UNIVERSE_PRIV_MASK  | UNIVERSE_VDW_MASK, 
	 UNIVERSE_VAS_A24   | UNIVERSE_PRIV_USER  | UNIVERSE_VDW_64, 
	 VME_AM_A24_MBLT(64)},
	{UNIVERSE_VAS_MASK  | UNIVERSE_PRIV_MASK  | UNIVERSE_VDW_MASK, 
	 UNIVERSE_VAS_A24   | UNIVERSE_PRIV_SUPER | UNIVERSE_VDW_64, 
	 VME_AM_A24_PRIV_MBLT(64)},
	{UNIVERSE_VAS_MASK  | UNIVERSE_PRIV_MASK  | UNIVERSE_BLT_MASK, 
	 UNIVERSE_VAS_A24   | UNIVERSE_PRIV_USER  | UNIVERSE_BLT, 
	 VME_AM_A24_BLT(0)},
	{UNIVERSE_VAS_MASK  | UNIVERSE_PRIV_MASK  | UNIVERSE_BLT_MASK, 
	 UNIVERSE_VAS_A24   | UNIVERSE_PRIV_SUPER | UNIVERSE_BLT, 
	 VME_AM_A24_PRIV_BLT(0)},
	{UNIVERSE_VAS_MASK  | UNIVERSE_PRIV_MASK  | UNIVERSE_SPACE_MASK, 
	 UNIVERSE_VAS_A24   | UNIVERSE_PRIV_USER  | UNIVERSE_SPACE_DATA, 
	 VME_AM_A24(0)},
	{UNIVERSE_VAS_MASK  | UNIVERSE_PRIV_MASK  | UNIVERSE_SPACE_MASK, 
	 UNIVERSE_VAS_A24   | UNIVERSE_PRIV_SUPER | UNIVERSE_SPACE_DATA, 
	 VME_AM_A24_PRIV(0)},
	{UNIVERSE_VAS_MASK  | UNIVERSE_PRIV_MASK  | UNIVERSE_SPACE_MASK, 
	 UNIVERSE_VAS_A24   | UNIVERSE_PRIV_USER  | UNIVERSE_SPACE_PROGRAM, 
	 VME_AM_A24_PROG(0)},
	{UNIVERSE_VAS_MASK  | UNIVERSE_PRIV_MASK  | UNIVERSE_SPACE_MASK, 
	 UNIVERSE_VAS_A24   | UNIVERSE_PRIV_SUPER | UNIVERSE_SPACE_PROGRAM, 
	 VME_AM_A24_PRIV_PROG(0)},
	{UNIVERSE_VAS_MASK  | UNIVERSE_PRIV_MASK  | UNIVERSE_VDW_MASK, 
	 UNIVERSE_VAS_A32   | UNIVERSE_PRIV_USER  | UNIVERSE_VDW_64, 
	 VME_AM_A32_MBLT(64)},
	{UNIVERSE_VAS_MASK  | UNIVERSE_PRIV_MASK  | UNIVERSE_VDW_MASK, 
	 UNIVERSE_VAS_A32   | UNIVERSE_PRIV_SUPER | UNIVERSE_VDW_64, 
	 VME_AM_A32_PRIV_MBLT(64)},
	{UNIVERSE_VAS_MASK  | UNIVERSE_PRIV_MASK  | UNIVERSE_BLT_MASK, 
	 UNIVERSE_VAS_A32   | UNIVERSE_PRIV_USER  | UNIVERSE_BLT, 
	 VME_AM_A32_BLT(0)},
	{UNIVERSE_VAS_MASK  | UNIVERSE_PRIV_MASK  | UNIVERSE_BLT_MASK, 
	 UNIVERSE_VAS_A32   | UNIVERSE_PRIV_SUPER | UNIVERSE_BLT, 
	 VME_AM_A32_PRIV_BLT(0)},
	{UNIVERSE_VAS_MASK  | UNIVERSE_PRIV_MASK  | UNIVERSE_SPACE_MASK, 
	 UNIVERSE_VAS_A32   | UNIVERSE_PRIV_USER  | UNIVERSE_SPACE_DATA, 
	 VME_AM_A32(0)},
	{UNIVERSE_VAS_MASK  | UNIVERSE_PRIV_MASK  | UNIVERSE_SPACE_MASK, 
	 UNIVERSE_VAS_A32   | UNIVERSE_PRIV_SUPER | UNIVERSE_SPACE_DATA, 
	 VME_AM_A32_PRIV(0)},
	{UNIVERSE_VAS_MASK  | UNIVERSE_PRIV_MASK  | UNIVERSE_SPACE_MASK, 
	 UNIVERSE_VAS_A32   | UNIVERSE_PRIV_USER  | UNIVERSE_SPACE_PROGRAM, 
	 VME_AM_A32_PROG(0)},
	{UNIVERSE_VAS_MASK  | UNIVERSE_PRIV_MASK  | UNIVERSE_SPACE_MASK, 
	 UNIVERSE_VAS_A32   | UNIVERSE_PRIV_SUPER | UNIVERSE_SPACE_PROGRAM, 
	 VME_AM_A32_PRIV_PROG(0)},
	{UNIVERSE_VDW_MASK, 
	 UNIVERSE_VDW_64,
	 ~0U},
	{UNIVERSE_BLT_MASK, 
	 UNIVERSE_BLT,
	 ~0U},
	{UNIVERSE_VAS_MASK  | UNIVERSE_PRIV_MASK,
	 UNIVERSE_VAS_A16   | UNIVERSE_PRIV_USER,
	 VME_AM_A16(0)},
	{UNIVERSE_VAS_MASK  | UNIVERSE_PRIV_MASK,
	 UNIVERSE_VAS_A16   | UNIVERSE_PRIV_SUPER,
	 VME_AM_A16_PRIV(0)},
	{UNIVERSE_VAS_MASK,
	 UNIVERSE_VAS_CRCSR,
	 VME_AM_CRCSR(0)},
	/* User reserved AM not supported, 2 more entries would have
	 * to be inserted here if they were. 
	 */
	{0,
	 0,
	 ~0U},
};

/* Table used to mask addresses when setting up root regions at driver
 * initialization (these are masks as well as limits).
 */
static const u32 
vas_masks[] = {
	0x0000ffff, 0x00ffffff, 0xffffffff, 0x00000000, 
	0x00000000, 0x00ffffff, 0xffffffff, 0xffffffff
};

/* By convention, there is a local variable called __rp pointing in all 
 * procedures requiring access to the Universe registers.
 */

/* These definitions are probably still not correct on all architectures. 
 * There are 2 sets: read and write are used to access I/O space, get and put 
 * to access in memory structures like the DMA descriptors. 
 */
#if defined(__powerpc__) 
#define __get_le32(addr) ({u32 tmp; \
		asm("lwbrx %0,0,%1" : "=r" (tmp): "r"(addr), "m"(*addr)); \
		tmp;})
#define __put_le32(val, addr) \
	asm("stwbrx %2,0,%1": "=m"(*addr) : "r"(addr), "r"(val))

#define __read_le16(addr) ld_le16(addr)
#define __read_le32(addr) ld_le32(addr)

#define __write_le16(val, addr) st_le16(addr, val)
#define __write_le32(val, addr) st_le32(addr, val)

#else

#define __get_le32(addr) le32_to_cpu(*(u32 *)(addr))
#define __put_le32(val,addr) *(u32 *)(addr) = cpu_to_le32(val);

#if defined(__BIG_ENDIAN)

#define __read_le16(addr) __le16_to_cpu(__read_be16(addr))
#define __read_le32(addr) __le32_to_cpu(__read_be32(addr))

#define __write_le16(val, addr) __write_be16(__cpu_to_le16(val), addr)
#define __write_le32(val, addr) __write_be32(__cpu_to_le32(val), addr)

#elif defined(__i386__)
/* Bypass all the ugly handling for legacy in Intel's read/write macros */

#define __read_le16(addr) (*(volatile u16 *)(addr))
#define __read_le32(addr) (*(volatile u32 *)(addr))

#define __write_le16(val, addr) (*(volatile u16 *)(addr)) = val;
#define __write_le32(val, addr) (*(volatile u32 *)(addr)) = val;

#else
/* May be optimal or not on Alpha and other little-endian machines */

#define __read_le16(addr) readw(addr)
#define __read_le32(addr) readl(addr)

#define __write_le16(val, addr) writew(val, addr)
#define __write_le32(val, addr) writel(val, addr)

#endif
#endif

/* Let us hide all these definitions under usable names */

#define GET_REG(off) \
	__read_le32((u32 *)(__rp+UNIVERSE_##off))

#define SET_REG(val, off) \
	__write_le32(val, (u32 *)(__rp+UNIVERSE_##off))

#define SET_REG_BE(val, off) \
	__write_be32(val, (u32 *)(__rp+UNIVERSE_##off))

#define GET_REG_HALF(off) \
	__read_le16((u16 *)(__rp+UNIVERSE_##off));
	
#define SET_REG_HALF(val, off) \
	__write_le16(val, (u16 *)(__rp+UNIVERSE_##off))

#define PCI_STATUS_RESET_MASK \
     (PCI_STATUS_PARITY|PCI_STATUS_SIG_TARGET_ABORT| \
      PCI_STATUS_REC_TARGET_ABORT|PCI_STATUS_REC_MASTER_ABORT| \
      PCI_STATUS_SIG_SYSTEM_ERROR|PCI_STATUS_DETECTED_PARITY)

/* This routine should be modified to suit the needs of the specific system.
 * It might be at some time replaced by module parameters, but the list
 * of parameters would be quite long. 
 */
static inline void
universe_setup(volatile u_char *__rp) {

	const u32 misc_mask = UNIVERSE_VBTO_MASK | UNIVERSE_VARB_PRIO | 
	  UNIVERSE_VARBTO_MASK | UNIVERSE_BI | UNIVERSE_ENGBI | 
	  UNIVERSE_RESCIND | UNIVERSE_V64AUTO;
	const u32 dgcs_mask = UNIVERSE_DGCS_CHAIN | UNIVERSE_DGCS_VON_MASK | 
	  UNIVERSE_DGCS_VOFF_MASK | UNIVERSE_DGCS_INTMASKS;
	u32 tmp;

	/* Set VME bus transfer and arbitration timeout to 16 microseconds,
	 * with priority arbitration. Reset all bus-isolation modes, rescind 
	 * DTACK (can't be disabled on revision 2 anyway), but do 
	 * not change the SYSCON setting.
	 * Avoid triggering the AUTO-ID mechanism. 
	 */
	tmp = GET_REG(MISC_CTL) & ~ misc_mask;
	tmp |= misc_mask & (UNIVERSE_VBTO_16uS | UNIVERSE_VARB_PRIO | 
			    UNIVERSE_VARBTO_16uS | UNIVERSE_RESCIND);

#ifdef SUICIDAL_VME_RESET
	SET_REG(tmp, MISC_CTL);
#else
	if (reset) {
		SET_REG(tmp | UNIVERSE_SW_SYSRST, MISC_CTL);
		udelay(2);
		SET_REG(tmp, MISC_CTL);
		current->state = TASK_UNINTERRUPTIBLE;
		schedule_timeout(HZ/3);
	} else {
		SET_REG(tmp, MISC_CTL);
	}	  
#endif

	/* This setup of mast_ctl is for single VMEBus system master, clearing
	 * the vown bit just in case. In single master mode, we are not
	 * affected by the bug on the MAXRTRY counter but we nevertheless 
	 * enable the workaround. All the other values (VRM_DEMAND, VREL_RWD
	 * and PABS32) are required to work around bugs. PABS64 may only be 
	 * set if no PCI to VME DMA is performed on Universe I!
	 */

	SET_REG(UNIVERSE_MAXRTRY_INFINITE | UNIVERSE_PWON_128 | 
		UNIVERSE_VRL(3) | UNIVERSE_VRM_DEMAND | UNIVERSE_VREL_RWD | 
		UNIVERSE_PABS_32,
	        MAST_CTL);

	/* Set VON and VOFF, VON is set to 512 because it can seriously 
	 * affect interrupt latency and number of retries for direct bus 
	 * operations. Let the DMA stop for 32uS between tenures. Enable 
	 * interrupts (Side effect: clearing all pending status bits STOP 
	 * to P_ERR). These choices allow to take an interrupt after
	 * a DMA tenure (since the interrupt acknowledge cycle will have
	 * the highest priority) and leave 32 uS until the interrupt 
	 * handlers start accessing the VME bus, then the CWT timer will keep
	 * the VME bus to the coupled channel until no more accesses are
	 * performed for a few microseconds, at this time, the DMA will
	 * restart operation for a full block before sampling interrupts
	 * again. These choices depend strongly on the application and
	 * should be tuned accordingly, they are simply aimed at providing
	 * reasonable default values.
	 * In the future, it might be possible to stop DMA while interrupt
	 * handlers are running to guarantee handlers a more deterministic
	 * access to the VME bus. The flags field in the struct vme_interrupt,
	 * currently unused, is actually designed to provide this kind of 
	 * information to the generic vme interrupt handler. This would allow
	 * optimizations like avoiding to suspend and restart the DMA when the 
	 * handler actually does not need VME access (which can only happen 
	 * with ROAK interrupts). But for now we don't even stop the DMA when
	 * an interrupt comes in: we simply hope that the accesses performed 
	 * by the handlers start before the VOFF timer expires and that 
	 * ownership of the VME bus is kept by the CWT timer.
	 */
	tmp = GET_REG(DGCS) & ~ dgcs_mask;

	tmp |= dgcs_mask & (UNIVERSE_DGCS_CHAIN | UNIVERSE_DGCS_VON_512 | 
		UNIVERSE_DGCS_VOFF_32uS | UNIVERSE_DGCS_INTMASKS);

	SET_REG(tmp, DGCS);

	/* Set up the miscellaneous PCI register, we set CRT to 1024uS
	 * because this is is similar to the fixed value (32768 PCI clocks)
	 * used by later revisions of the chip. And set CWT to 128 PCI clocks 
	 * (almost 4 us). 
	 */
	SET_REG(UNIVERSE_CRT_1024uS | UNIVERSE_CWT_128, LMISC);


#ifdef UNIVERSE_IMAGE_SETUP
	/* Setting up slave images should have been performed earlier by a 
	 * resource allocator, but not all boards have this capability.
	 * In this case, the following code should be edited to suit your 
	 * needs. 
	 */
	/* The following is an outdated description, it is only valid
	 * on PPC without the powerplus special setup. 
	 */
	/* Set up the Special slave image with the following:
	 * - mapped at the highest possible adress of BAT1: 0xCC000000 
	 *   on PPC side, corresponding to 0x0C000000 on PCI, or 3 times 
	 *   64 Mbytes,
	 * - first block: user mode, 16 bits maximum data width,
	 * - second block: user mode, 32 bits width,
	 * - third block: supervisor mode, 16 bits maximum data width, 
	 * - fourth block: supervisor mode, 32 bits width,
	 * All images generate data space access (I don't expect anybody
	 * to run code through the VMEBus, if you need to do it first copy
	 * it to local memory and then execute it there since it can be cached
	 * and executes much faster).
	 * For now write posting is disabled because it may result in bus
	 * lockups due to errata in Universe V1, but it may be tested and
	 * won't affect the functionality of the driver.
	 */
	/* Don't want to export a symbol just for this, actually this should
	 * be setup by firmware according to user's needs.
	 */
#ifndef __powerpc__
#define pwrplus 0
#else 
#define pwrplus ((_machine == _MACH_prep) && (universe.bus_delta == 0))
#endif
	SET_REG(UNIVERSE_SLAVE_EN |
		UNIVERSE_SLSI_VDW_32*((1<<1)+(1<<3)) |
		UNIVERSE_SLSI_PRIV_SUPER*((1<<2)+(1<<3)) |
	        ((pwrplus ? 0xf4000000 : 0xcc000000)
		 -universe.bus_delta)>>UNIVERSE_SLSI_BS_ADDR_SHIFT,
		SLSI);


	/* Set up image 0 base and bound (8kB) */
	SET_REG((pwrplus ? 0xfc800000 : 0xc1ffe000 - universe.bus_delta),
		LSI_BS(0));
	SET_REG((pwrplus ? 0xfc802000 : 0xc2000000 - universe.bus_delta), 
		LSI_BD(0));
	/* TO and CTL are dynamically programmed according to needs */

	/* Set up the CR/CSR address space for tests */
	SET_REG((pwrplus ? 0xf3000000 : 0xc2000000 - universe.bus_delta),
		LSI_BS(1));
	SET_REG((pwrplus ? 0xf4000000 : 0xc3000000 - universe.bus_delta),
		LSI_BD(1));
	SET_REG(0x00000000, LSI_TO(1));
	SET_REG(UNIVERSE_SLAVE_EN | UNIVERSE_VAS_CRCSR | UNIVERSE_VDW_32,
		LSI_CTL(1));

	/* Set up an A32/D32 image with BLT for tests */
	SET_REG((pwrplus ? 0xe0000000: 0xc8000000 - universe.bus_delta), 
		LSI_BS(2));
	SET_REG((pwrplus ? 0xf3000000: 0xcc000000 - universe.bus_delta), 
		LSI_BD(2));
	SET_REG(universe.bus_delta - 0xc8000000, LSI_TO(2));
	SET_REG(UNIVERSE_SLAVE_EN | UNIVERSE_VAS_A32 | UNIVERSE_VDW_32,
		/* | UNIVERSE_BLT,*/
		LSI_CTL(2));
#endif /* UNIVERSE_IMAGE_SETUP */

	}



#ifdef UNIVERSE_DEBUG
#define dprintk(args...) printk(KERN_DEBUG ##args)
#define print_register(reg) printk(KERN_DEBUG \
				   "register " #reg " =%8.8x\n",GET_REG(reg))
#define print_register_i(reg, i) printk(KERN_DEBUG \
					"register " #reg "(%d) =%8.8x\n", \
					i, GET_REG(reg(i)))

     
static void print_registers(void)
{
  	volatile u_char *__rp=universe.reg_base;
	int i, limit;
	if (universe.revision) limit=8; else limit=4;
	for (i=0; i<limit; i++) {
		if (GET_REG(LSI_CTL(i)) & UNIVERSE_SLAVE_EN) {
			print_register_i(LSI_CTL, i);
			print_register_i(LSI_BS, i);
			print_register_i(LSI_BD, i);
			print_register_i(LSI_TO, i);
		}
	}
	print_register(SCYC_CTL);
	print_register(LMISC);
	print_register(SLSI);
	print_register(LERRLOG);
	print_register(DCTL);
	print_register(DGCS);
	print_register(D_LLUE);
	print_register(LINT_EN);
	print_register(LINT_STAT);
	print_register(VINT_EN);
	print_register(VINT_STAT);
	print_register(V1_STATID);
	print_register(V2_STATID);
	print_register(V3_STATID);
	print_register(V4_STATID);
	print_register(V5_STATID);
	print_register(V6_STATID);
	print_register(V7_STATID);
	print_register(MAST_CTL);
	print_register(MISC_CTL);
	print_register(MISC_STAT);
	for (i=0; i<limit; i++) {
		if (GET_REG(VSI_CTL(i)) & UNIVERSE_SLAVE_EN) {
			print_register_i(VSI_CTL, i);
			print_register_i(VSI_BS, i);
			print_register_i(VSI_BD, i);
			print_register_i(VSI_TO, i);
		}
	}
	print_register(VRAI_CTL);
	print_register(VCSR_CTL);
	print_register(VERRLOG);
	print_register(VCSR_CLR);
	/* Here more should be added for Universe II */
}
#else
#define dprintk(args...)
#define print_registers()
#endif

int 
vme_alloc_dmalist(struct vme_device *dev, struct vme_dma *dma, 
		 size_t maxfrags) {
	u_long fl;
	int retval;
	void * v;
	if (maxfrags > (PAGE_SIZE/sizeof(struct universe_dma_entry)) ||
	    maxfrags <=0 || !dma->handler) return -EINVAL;
	v = kmalloc(maxfrags * sizeof(struct universe_dma_entry), 
		    GFP_KERNEL);
	if (v == NULL) return -ENOMEM;
	write_lock_irqsave(&universe.lock, fl);
	if (dma->device) { 
	  	kfree(v); 
		retval = -EBUSY;
		goto out;
	}
	dma->private = v;
	dma->maxfrags = maxfrags;
	dma->flags = 0;
	dma->device = dev;
	
	dma->next = dev->dmalists;
	dev->dmalists = dma;
	dma->flags = 1<<VME_DMA_READY; /* This clears BUSY just in case */
	retval = 0;
 out:	write_unlock_irqrestore(&universe.lock, fl);
	return retval;
}

void
vme_free_dmalist(struct vme_dma *dma) {
	u_long fl;
	int wasready;

	spin_lock_irqsave(&universe.dma_lock, fl);
	if ((wasready=test_and_clear_bit(VME_DMA_READY, &dma->flags))
	    && test_bit(VME_DMA_BUSY, &dma->flags)) {
		/* We could optimize by searching the list and removing the
		 * entry if it is in the queue of pending operations. But
		 * if the dma list is being setup by queue_dmalist it will
		 * not be found. So the worst case remains the same 
		 * and only the simple method is implemented: being
		 * woken up when the dma has finished. 
		 */
		spin_unlock_irqrestore(&universe.dmalock, fl);
		wait_event(universe.dma_free_wait, 
			   !test_bit(VME_DMA_BUSY, &dma->flags));
	} else {
		spin_unlock_irqrestore(&universe.dmalock, fl);
		if (!wasready) return;
	}

	/* Now the dmalist can be freed */
	write_lock_irqsave(&universe.lock, fl);
	if (!dma->device) goto out;
	if (dma->device->dmalists == dma) {
		dma->device->dmalists = dma->next;
	} else {
		struct vme_dma *p = dma->device->dmalists;
		for (; p && p->next != dma; p=p->next);
		if (!p) goto out;
		p->next = p->next->next;
	}
	dma->device = NULL;
	dma->queue = NULL;
  	kfree (dma->private); 
	dma->private = NULL;
 out:	write_unlock_irqrestore(&universe.lock, fl);
}


int 
vme_queue_dmalist(struct vme_dma *dma, struct vme_dmavec *dv, size_t frags) {
	u_long fl;
	struct universe_dma_entry *p;
	size_t totlen = 0;
	int retval;
	/* The locking is quite complex because of interaction with
	 * vme_free_dmalists. The BUSY bit does not need the lock
	 * when cleared, and READY when set. All other transitions
	 * require dma_lock to be acquired first.
	 */
	spin_lock_irqsave(&universe.dma_lock, fl);
	retval = -EINVAL;
	/* Prevent multiple simultaneous uses of the same DMA list or
	 * queuing a list which is not READY (may mean being freed).
	 */
	if (frags > dma->maxfrags || frags <= 0 || dma->timeout<2 
	    || !test_bit(VME_DMA_READY, &dma->flags)) { 
		goto out;
	} 
	retval = -EBUSY;
	if (test_and_set_bit(VME_DMA_BUSY, &dma->flags)) goto out;
	spin_unlock_irqrestore(&universe.dma_lock, fl);

	/* Now fill the Universe scatter gather list */
	for (p = dma->private; frags!=0; p++, dv++, frags--) {
		u32 dctl = am2ctl[(dv->flags & VME_AM_MASK)>>VME_AM_SHIFT] &
		  dw2ctl[(dv->flags&VME_DW_MASK)>>VME_DW_SHIFT];

		/* some sanity checks: only one DW flag bit is set, the 
		 * Universe DMA byte counter has only 24 bits (it's not a 
		 * limitation since you can't allocate enough contiguous 
		 * kernel memory anyway), the data width must be compatible 
		 * with the AM code, DMA must be supported for this AM code, 
		 * and the low 3 bit of addresses must be the same. 
		 */
		if (!(dctl & UNIVERSE_SLAVE_EN) || 
		    (dv->length & 0xff000000) ||
		    (am_bad_dw[(dv->flags & VME_AM_MASK)>>VME_AM_SHIFT] 
		      & dv->flags) ||
		    (am_bad_use[(dv->flags & VME_AM_MASK)>>VME_AM_SHIFT]
		     & VME_USE_DMA) ||
		    ((dv->kvaddr ^ dv->vme_addr) & 7)) return -EINVAL;
		totlen += dv->length;

		dctl = (dctl & ~UNIVERSE_SLAVE_EN) | 
		  (((dv->flags&VME_DMA_DIRECTION_MASK)
		    >>VME_DMA_DIRECTION_SHIFT)<<UNIVERSE_DMA_L2V_SHIFT) |
		  UNIVERSE_USEPCI64;

		p->__res01 = p->__res02 = p->__res03 = 0;
		__put_le32(dv->vme_addr, &p->vaddr);
		__put_le32(virt_to_bus((void *)dv->kvaddr), &p->laddr);
		__put_le32(dv->length, &p->dtbc);
		__put_le32(dctl, &p->dctl);
		__put_le32((frags == 1) ? 1 : virt_to_bus(p+1), &p->dcpp);
	}
	dma->remlen = totlen;
	dma->queue = NULL;

	spin_lock_irqsave(&universe.dma_lock, fl);
	if (universe.dma_queue_end) {
		universe.dma_queue_end->queue = dma;
	} else {
		universe.dma_queue = dma;
	  	/* We have to start the DMA in the bridge since it's 
		 * stopped unless the interrupt handler, which will 
		 * take care of starting it, is running.
		 */
		if (!test_bit(IRQ_ACTIVE,&universe.state)) {
		  	/* Now the interrupt may come on another processor
			 * and set IRQ_ACTIVE to prevent interfering with 
			 * PIO bus accesses while the interrupt is running. 
			 * It's too late, the DMA will be started...
			 */
			volatile u_char * __rp = universe.reg_base;
			SET_REG(0, DTBC);
			SET_REG(virt_to_bus(dma->private), DCPP);
			iobarrier_w();
			SET_REG(universe.cached_dgcs | 
				UNIVERSE_DGCS_GO, DGCS);
			universe.dma_timer.expires = jiffies + dma->timeout;
			add_timer(&universe.dma_timer);
			set_bit(DMA_ACTIVE, &universe.state);
		}
	}
	universe.dma_queue_end = dma;
	retval = 0;
 out:	spin_unlock_irqrestore(&universe.dma_lock,fl);
	return retval;
}

#ifdef UNIVERSE_DEBUG
static void inline
dump_sglist(struct universe_dma_entry *p) {
	printk(KERN_DEBUG "  Start of list: \n");
  	do {
		printk("    dctl:  %08x\n    dtbc:  %08x\n"
		       "    laddr: %08x\n    vaddr: %08x\n"
		       "    dcpp:  %08x\n", 
		       __get_le32(&p->dctl), __get_le32(&p->dtbc),
		       __get_le32(&p->laddr), __get_le32(&p->vaddr),
		       __get_le32(&p->dcpp));
		if (__get_le32(&p->dcpp) & 1) break;
		p = bus_to_virt(__get_le32(&p->dcpp) & ~7);
	} while(1);
	printk(KERN_DEBUG "  End of list.\n");
}

/* Actually this one should be protected with spinlocks, but it was only
 * used to check that the Universe dma descriptor lists were correctly built.
 */
void
vme_dump_dmalists(void) {
	struct vme_device * dev;
	printk(KERN_DEBUG "Dumping DMA lists...\n");
	for (dev=&vme_root; dev; dev=dev->next) {
		struct vme_dma * dma;
		if (dev->dmalists == NULL) continue;
		printk (KERN_DEBUG "DMA lists for device %s.\n", dev->name); 
		for(dma=dev->dmalists; dma; dma=dma->next) {
			if (dma->private) dump_sglist(dma->private);
		}
	}
}
#endif
/* Looking for an image with the specified attributes: note that we ask
 * for an exact match on the addressing space (AM code), but the caller may 
 * not care about a precise data width attribute. For example to access a D8(O)
 * slave you don't care whether the Universe might generate 16 or 32 bit 
 * accesses so any image with DW=8, 16 or 32 is suitable. 
 * That's why there are 4 bits and not 2 for data width attributes and they
 * are said to match if the and of image and requested flags is non zero.
 *
 * IMPORTANT: This function is called after having checked
 * that size is > 0 and that there is no wraparound ! 
 */
static struct vme_region * 
find_root_region(u_int flags, u_long base, u_long limit) {
	struct vme_region * p;

	for ( p = vme_root.regions; p; p = p->next) {
		if ((flags ^ p->flags) & VME_AM_MASK) continue;
		if (!(flags & p->flags & VME_DW_MASK)) continue;
		/* Don't remove the -1, they are required for correctness
		 * if an image maps the high end of the 32 bit address space!
		 */
		if ((base >= p->base) && 
		    (limit <= p->limit)) break;
	}
	return p;
}

/* VME_USE_PIO means that the kvaddr has to be set or return an error, 
 * VME_USE_MAP means that the region has been mapped by ioremap and iounmap 
 * should be performed when freed by unregister region. Otherwise, it is a 
 * subset of a permanently mapped region. VME_USE_RMW has its standard meaning
 * but is not actually enforced in the corresponding code (it's kernel code, 
 * you should know what you are doing). VME_USE_DMA set means the region is 
 * used for DMA and will show as such in /proc/bus/vme/regions. 
 * 
 */

int
vme_register_region(struct vme_device *dev, struct vme_region *reg) {
	u_long fl;
	struct vme_region *p;
	u32 flags =  reg->flags & ~VME_USE_MAP;
	volatile u_char * kvaddr;
	int retval;
	
	/* Check a) that at least one DW bit is set, b) that if it is not a 
	 * PIO only region, only one of the DW bits is set, c) that DW is 
	 * valid for this AM, d) that no VME_USE flag incompatible with
	 * AM is set, e) that the range is non empty, and f) that the range
	 * does not exceed the limit for this AM. 
	 */
	if (!(VME_DW_MASK & flags) ||
	    (((flags & (VME_USE_DMA | VME_USE_PIO)) != VME_USE_PIO) && 
	     !(dw2ctl[(flags & VME_DW_MASK)>>VME_DW_SHIFT])) ||
	    (am_bad_dw[(flags & VME_AM_MASK)>>VME_AM_SHIFT] & flags) ||
	    (am_bad_use[(flags & VME_AM_MASK)>>VME_AM_SHIFT] & flags) ||
	    (reg->limit < reg->base) || 
	    (reg->limit >
	     vas_masks[(am2ctl[(flags & VME_AM_MASK) >> VME_AM_SHIFT]
			&UNIVERSE_VAS_MASK) >> UNIVERSE_VAS_SHIFT]))
	  return -EINVAL;

	/* If requested, map the area in the kernel virtual memory */
	p = 0;
	kvaddr=0;
	if (flags & VME_USE_PIO) {
		p = find_root_region(reg->flags, reg->base, reg->limit);
		if (!p) return -ENXIO;
		/* Mask the DW flags with the one actually used */
		flags &= p->flags | ~VME_DW_MASK;
		/* Don't ioremap permanently mapped areas */
		if (p->flags & VME_USE_PIO) {
			kvaddr = p->kvaddr + (reg->base - p->base);
		} else {
			kvaddr = ioremap(p->phyaddr + (reg->base - p->base),
					  reg->limit + 1 - reg->base);
			/* This may not be the best error code but it is very
			 * likely to be the cause.
			 */
			if (!kvaddr) return -ENOMEM;
			/* Record that iounmap will have to be done */
			flags |= VME_USE_MAP;
		}
	}
	write_lock_irqsave(&universe.lock, fl);
	if (reg->device) {
		if (flags & VME_USE_MAP) iounmap((void *)kvaddr);
		retval = -EBUSY;
		goto out;
	}
	reg->phyaddr = p ? p->phyaddr + (reg->base - p->base) : 0;
	reg->flags = flags;
	reg->kvaddr = kvaddr;
	reg->device = dev;
	reg->next = dev->regions;
	dev->regions = reg;
	retval = 0;
 out:	write_unlock_irqrestore(&universe.lock, fl);
	return retval;
}

void
vme_unregister_region(struct vme_region *reg) {
	u_long fl;

	write_lock_irqsave(&universe.lock, fl);
	if(!reg->device) goto out;
	if(reg->device->regions == reg) {
		reg->device->regions = reg->next;
	} else {
		struct vme_region *p = reg->device->regions;
		for (; p && p->next != reg; p=p->next);
		if (p) p->next = p->next->next;
	}
	reg->device = NULL;
	if (reg->flags & VME_USE_MAP) {
		iounmap((void *)reg->kvaddr);
	}
	reg->kvaddr = 0;
	reg->phyaddr = 0;
 out:	write_unlock_irqrestore(&universe.lock, fl);
}

int
vme_request_interrupt(struct vme_device *dev, struct vme_interrupt *intr) {
	int retval;
	u_long fl;
	volatile u_char * __rp = universe.reg_base;

	if (intr->level <1 || intr->level>7 || 
	    intr->vector>255 || !intr->handler) 
	  	return -EINVAL;
	/* If the interrupt level has been permanently disabled because 
	 * of a serious problem return -EBUSY. There might be a better error
	 * code. Strange enough valid_virqs is only modified by the interrupt
	 * handler which acquires the read_lock, and here is the only place
	 * where it is tested is with the write lock held, but it works...
	 */
	write_lock_irqsave(&universe.lock, fl);
	retval = -EBUSY;
	if ((universe.valid_virqs & (1<<(intr->level))) && (!intr->device) && 
	    (universe.ints[intr->level-1][intr->vector] == 0)) {
		intr->device = dev;
		intr->next = dev->interrupts;
		dev->interrupts = intr;
		intr->count = 0;
		universe.ints[intr->level-1][intr->vector] = intr;
		universe.virq[intr->level-1].handlers++;
		if (universe.virq[intr->level-1].handlers == 1) {
		  	universe.cached_lint_en |= 1<<(intr->level);
			SET_REG(universe.cached_lint_en, LINT_EN);
		}
		retval = 0;
	}
	write_unlock_irqrestore(&universe.lock, fl);
	return retval;
}

void
vme_free_interrupt(struct vme_interrupt *intr) {
	u_long fl;
	volatile u_char * __rp = universe.reg_base;

	write_lock_irqsave(&universe.lock, fl);
	if (!intr->device) goto out;
	if(intr->device->interrupts == intr) {
		intr->device->interrupts = intr->next;
	} else {
		struct vme_interrupt *p = intr->device->interrupts;
		for (; p && p->next != intr; p=p->next);
		if (p) p->next = p->next->next;
	}
	intr->device = NULL;
	universe.ints[intr->level-1][intr->vector]=NULL;
	universe.virq[intr->level-1].handlers--;
	if (universe.virq[intr->level-1].handlers == 0) {
		universe.cached_lint_en &= ~(1<<(intr->level));
		SET_REG(universe.cached_lint_en, LINT_EN);
	}
 out:	write_unlock_irqrestore(&universe.lock, fl);
}

static inline void
vme_remove_resources(struct vme_device *dev) {
	while (dev->dmalists) vme_free_dmalist(dev->dmalists);
	while (dev->regions) vme_unregister_region(dev->regions);
	while (dev->interrupts) vme_free_interrupt(dev->interrupts);
}

int 
vme_register_device(struct vme_device * dev) {
	u_long fl;
	int retval = -EBUSY;
	struct vme_device *p;
	/* Insert in sorted by minor number: it will fail when trying 
	 * to register twice the same device. Note that device that set
	 * the minor to 0 and fileops to NULL are allowed: it means
	 * that they are not opened through this driver. 
	 */
	write_lock_irqsave(&universe.lock, fl);
	if (dev->fops || dev->minor) {
		for (p= &vme_root; p->next; p=p->next) {
			if (p->next->minor > dev->minor) break;
			if (p->next->minor == dev->minor) goto out;
		}
	} else {
		for(p= &vme_root; p->next; p=p->next);
	}
	dev->next = p->next;
	p->next = dev;
	retval = 0;
 out:	write_unlock_irqrestore(&universe.lock, fl);
	return retval;
}

void 
vme_unregister_device(struct vme_device *dev) {
	u_long fl;
	struct vme_device *p;

	write_lock_irqsave(&universe.lock, fl);
	/* Note that we can't unregister the root vme device, 
	 * but that's exactly what we want.
	 */
	for(p=&vme_root; p->next && p->next!=dev; p=p->next);
	if (p->next == dev) { 
		p->next=dev->next;
	}
	write_unlock_irqrestore(&universe.lock, fl);
	vme_remove_resources(dev);
	return;
}

static int 
universe_open(struct inode * inode, struct file * file)
{
  	/* Note that this call is not protected by spinlocks, so that
	 * the open function of the device may register/unregister regions,
	 * interrupts and dma lists. Right now open and close are always 
	 * interlocked with module loading/unloading. Changes will be required
	 * if this is not the case in the future (many operations in this 
	 * driver are already protected against concurrent operations because
	 * most of the structures may be read during interrupts).
	 */
	 
        int minor = MINOR(inode->i_rdev);
	struct vme_device *p;

	for (p=&vme_root; p && p->minor!=minor; p=p->next);

	if (!(p && p->fops && p->fops->open)) return -ENODEV;

	if (p==&vme_root) {
		struct vme_region * f;
		f = (struct vme_region *) kmalloc(sizeof(*f), GFP_KERNEL);
		if (!f) return -ENOMEM;
		/* A SET_ATTR ioctl is necessary before:
		 * any access may be performed.
		 */
		MOD_INC_USE_COUNT;
		f->kvaddr = 0;
		f->phyaddr = 0;
		f->base = 1;
		f->limit = 0;
		f->flags = 0;
		f->next = NULL;
		f->device = &vme_root;
		file->private_data = f;
		return 0;
	} else {
	  	file->f_op = p->fops;
		return file->f_op->open(inode, file);
	}
} 

static int 
universe_release(struct inode * inode, struct file * file)
{
  	/* kfree is a nop when called with a NULL pointer */
	kfree(file->private_data);
  	MOD_DEC_USE_COUNT;
	return 0;
}

static loff_t universe_llseek(struct file *file, loff_t offset, int origin) {
	struct vme_region * FPD = (struct vme_region *) file->private_data;
	loff_t retval;
	switch(origin) {
	case 2:
	  offset += FPD->limit + 1 - FPD->base;
	  break;
	case 1:
	  offset += file->f_pos;
	  break;
	}
	retval = -EINVAL;
	if (offset>=0) {
		retval = file->f_pos = offset;
	}
	return retval;
};

static ssize_t 
universe_read(struct file *file, char * buf, size_t count, loff_t *ppos) {
	struct vme_region * FPD = (struct vme_region *) file->private_data;
	ssize_t retval;
	size_t maxpos = FPD->limit - FPD->base;

	if (!(FPD->flags&VME_USE_PIO)) return -EINVAL;
  	/* Check that position and counts are ok */
	if (*ppos > maxpos) return 0;
	if ( count > (maxpos - *ppos)) 
		count = maxpos - *ppos;
	
  	/* Then check for bad buffer */
	if (!access_ok(VERIFY_WRITE, buf, count)) return -EFAULT;

	/* We use here an arch specific routine which might not
	 * work with other machines since byteorder of PPC is same
	 * as VME.
	 */
#ifdef __powerpc__
	retval = copy_io_to_user(buf, FPD->kvaddr + *ppos, count);
	return retval ? -EFAULT : count;
#else
#warning "No copy_io_to_user routine supported in this architecture for now."
	return -EINVAL;
#endif
}

static ssize_t 
universe_write(struct file *file, const char *buf, size_t count, loff_t *ppos){
	struct vme_region * FPD = (struct vme_region *) file->private_data;
	ssize_t retval;
	size_t maxpos = FPD->limit - FPD->base;

	if (!(FPD->flags&VME_USE_PIO)) return -EINVAL;
  	/* Check that position and counts are ok */
	if (*ppos > maxpos) return 0;
	if ( count > (maxpos - *ppos)) 
		count = maxpos - *ppos;
	
  	/* Then check for bad buffer */
	if (!access_ok(VERIFY_READ, buf, count)) return -EFAULT;

	/* We use here an arch specific routine which might not
	 * work with other machines since byteorder of PPC is same
	 * as VME.
	 */
#ifdef __powerpc__
	retval = copy_user_to_io(FPD->kvaddr + *ppos, buf, count);
	return retval ? -EFAULT : count;
#else
#warning "No copy_user_to_io routine supported in this architecture for now."
	return -EINVAL;
#endif
}


static int 
universe_mmap(struct file *file, struct vm_area_struct *vma) {
	struct vme_region * FPD = (struct vme_region *) file->private_data;
	u_long off = vma->vm_pgoff * PAGE_SIZE;
	u_long len = vma->vm_end - vma->vm_start;
	/* Caution: this might fail badly when PAGE_SIZE is 8k or more
	 * and Universe's images of 4kB resolution (0 and 4) are used.
	 * But this should be solved somewhere else to force mapping
	 * using these images with PAGE_SIZE alignment and size.
	 * Note also that do_mmap ensures that off + len does not wraparound
	 * so that this case is not checked here.
	 */
	if (!(FPD->flags & VME_USE_MAP) ||
	    (off + FPD->base) & ~PAGE_MASK) return -EINVAL;
	
	if ((off + len - 1) > (FPD->limit - FPD->base)) return -ENXIO;

	pgprot_val(vma->vm_page_prot) = 
	  pgprot_noncached(pgprot_val(vma->vm_page_prot));

	vma->vm_flags |= VM_IO;
	if (remap_page_range(vma->vm_start, (u_long) FPD->phyaddr+off, 
			     len, vma->vm_page_prot)) return -EAGAIN;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,2,3)
	/* This is now properly done in mm/mmap.c */
	vma->vm_file=file;
	file->f_count++;
#endif
	return 0;
}; 

/* This function works for now because there is no HW error reporting through
 * machine checks or similar mechanisms.
 */ 
int
vme_safe_access(u_int cmd, u32 flags, u_long vme_addr, u_long *value) { 
	volatile u_char *__rp = universe.reg_base, *vp;
	u_long fl;
	unsigned short status;
	int retval;
	u32 ctl = am2ctl[(flags & VME_AM_MASK)>>VME_AM_SHIFT] &
	  dw2ctl[(flags&VME_DW_MASK)>>VME_DW_SHIFT];

	/* Check that DW is valid for this AM */
	if (!(ctl & UNIVERSE_SLAVE_EN) ||
	    (am_bad_dw[(flags & VME_AM_MASK)>>VME_AM_SHIFT] & flags))
	  return -EINVAL;
	
	vp = universe.root_regions[0].kvaddr + (vme_addr & 0xfff);
	/* On SMP, the following code guarantees only that, if there are no 
	 * bus errors on the access, the device is responding. But since 
	 * devices can be mmapped from user mode, the only safe way to check 
	 * for bus errors would be to put all other processors in a kernel
	 * mode idle loop. There is currently no way to do this (it would 
	 * probably require a special IPI and it would in any case be bad 
	 * for performance). Note these functions are not designed to be used 
	 * often, only to make sure that the hardware is present, for 
	 * diagnostic purposes, and when an operation requires interlock with
	 * atomic (vme_modbits) calls, so performance is not a major concern.
	 */ 
	dprintk("vme_safe_access: cmd=%x, flags=%x, vme_addr=%x\n",
		cmd, flags, vme_addr); 
	spin_lock_irqsave(&universe.bus_lock, fl);
	SET_REG(ctl, LSI_CTL(0));
	SET_REG((vme_addr-universe.root_regions[0].phyaddr) & ~0xfff, 
		LSI_TO(0));
	SET_REG_HALF(PCI_STATUS_SIG_TARGET_ABORT, PCI_STATUS);
	iobarrier_rw(); /* Needed for VME_READ cases */
	switch(cmd) {
	case VME_READ8:
	  	*value = vme_read8(vp);
		break;
	case VME_READ16:
		*value = vme_read16(vp);
		break;
	case VME_READ32:
		*value = vme_read32(vp);
		break;
	case VME_WRITE8:
		vme_write8(*value, vp);
		break;
	case VME_WRITE16:
		vme_write16(*value, vp);
		break;
	case VME_WRITE32:
		vme_write32(*value, vp);
		break;
	default:
		retval = -EINVAL;
	  	goto out;
	}
	iobarrier_rw(); /* Needed for VME_WRITE cases */
	status = GET_REG_HALF(PCI_STATUS);
	retval = 0;
 out:	spin_unlock_irqrestore(&universe.bus_lock, fl);
	dprintk("vme_safe_access: ctl=%x, to= %x, value=%x\n",
	       ctl, (vme_addr-universe.root_regions[0].phyaddr) & ~0xfff, 
	       *value); 
	if (!retval && status&PCI_STATUS_SIG_TARGET_ABORT) retval = -EIO;
	return retval;
}

/* This should always use true VME RMW cycles, but there are so many bugs in 
 * the Universe V1 that a workaround has been implemented exactly as suggested
 * by the errata except for the fact that we don't lock the VMEBus with the 
 * VOWN bit, a non-issue in single master environments and even in multimaster
 * environments when the CWT timer is not disabled (we set it to about 
 * 4 microseconds by default) and the PCI bus latency is not too high.  
 *
 * Note that the Universe way of handling RMW cycles may not always be 
 * what users want: the comparison is done on a bit by bit basis so that it 
 * makes for example the emulation of a compare and swap impossible.
 * 
 * That makes the compare data register in the special cycle generator 
 * redundant: actually it may be set to the complement of the new data
 * to be written and each unmasked bit will be set as desired.
 */

/* Warning: this function is not yet stabilized, its parameters and operation
 * are still likely to change in the future. 
 */

int
vme_modbits(u_int cmd, u32 flags, u_long vme_addr,
	    u_int *value, u_int mask) {
	volatile u_char *__rp = universe.reg_base, 
	  *vp = universe.root_regions[0].kvaddr + (vme_addr & 0xfff);
	u_long fl, size = 1<<(_IOC_NR(cmd)-_IOC_NR(VME_MODBITS8));
	int retval;
	u32 ctl = am2ctl[(flags & VME_AM_MASK)>>VME_AM_SHIFT] &
	  dw2ctl[(flags&VME_DW_MASK)>>VME_DW_SHIFT];


	/* Check that only one DW bit is set, that DW is valid for this AM, 
	 * that RMW cycle are actually allowed for this flag settings, that 
	 * size is not larger than the used VME bus width, and that the access
	 * is properly aligned. Experience shows that RMW cycles also work 
	 * with the special slave image (which is not obvious from the 
	 * documentation), but it is irrelevant here. 
	 */
	if (!(ctl & UNIVERSE_SLAVE_EN) ||
	    (am_bad_dw[(flags & VME_AM_MASK)>>VME_AM_SHIFT] & flags) ||
	    (am_bad_use[(flags & VME_AM_MASK)>>VME_AM_SHIFT] & 
	     VME_USE_RMW) ||
	    (size > ((flags & VME_DW_MASK)>>VME_DW_SHIFT)) ||
	    vme_addr & (size-1)) return -EINVAL;

  	spin_lock_irqsave(&universe.bus_lock, fl);
	SET_REG(ctl, LSI_CTL(0));
	SET_REG((vme_addr-universe.root_regions[0].phyaddr) & ~0xfff, 
		LSI_TO(0));
	vp = universe.root_regions[0].kvaddr + (vme_addr & 0xfff);
	retval = 0;
	if (universe.revision) {
		/* Using the special cycle generator, note that we don't care 
		 * about the order while setting the special cycle
		 * generator so we don't need any iobarrier: if the 
		 * processor or the host bridge attempts to perform 64 bit 
		 * tranfers through write combining, the Universe will not 
		 * acknowledge them and the tranfer will automatically fall 
		 * back to a series of non burst 32 bit accesses. This should 
		 * also be correct on little endian processors but has not 
		 * been tested.
		 */
		int shcnt = 32 - ((size+vme_addr)&3)*8;
		SET_REG_BE(mask<<shcnt, SCYC_EN);
		SET_REG_BE(*value<<shcnt, SCYC_SWP);
		SET_REG_BE(~(*value<<shcnt), SCYC_CMP);

		SET_REG(universe.root_regions[0].phyaddr + 
			(vme_addr & 0xffc), SCYC_ADDR);
		SET_REG(UNIVERSE_SCYC_RMW, SCYC_CTL);
		iobarrier_rw(); /* Needed since next access is read */
		switch (cmd) {
		case VME_MODBITS8:
			*value = vme_read8(vp);
			break;
		case VME_MODBITS16:
			*value = vme_read16(vp);
			break;
		case VME_MODBITS32:
			*value = vme_read32(vp);
			break;
		default:
			retval = -EINVAL;
			break;
		}
		/* reset SCYC_CTL to avoid unexpected behaviour */
		SET_REG(UNIVERSE_SCYC_DISABLE, SCYC_CTL);
	} else {
		u_long readval;
		iobarrier_rw();
		switch (cmd) {
		case VME_MODBITS8:
			readval = vme_read8(vp);
			vme_write8((*value & mask) | (readval & ~mask), vp);
			*value = readval;
			break;
		case VME_MODBITS16:
			readval = vme_read16(vp);
			vme_write16((*value & mask) | (readval & ~mask), vp);
			*value = readval;
			break;
		case VME_MODBITS32: 
			readval = vme_read32(vp);
			vme_write32((*value & mask) | (readval & ~mask), vp);
			*value = readval;
			break;
		default:
			retval = -EINVAL;
			break;
		}
	}
	iobarrier_rw();
	spin_unlock_irqrestore(&universe.bus_lock, fl);
	return retval;
}

static int 
universe_ioctl(struct inode * inode, struct file * file, u_int cmd, u_long arg)
{
	struct vme_region * FPD = (struct vme_region *) file->private_data;
  	u_int flags, rmwval;
	int error, size;
	u_long base, limit, offset, value, mask;
	struct vme_region *reg;
	
	if (_IOC_SIZE(cmd) && 
	    !access_ok((_IOC_DIR(cmd) & _IOC_READ) ? VERIFY_WRITE:VERIFY_READ,
		       arg, _IOC_SIZE(cmd))) return -EFAULT;
	switch(cmd) {
#define param ((VME_attr *) arg)
	case VME_SET_ATTR:
	  	error = __get_user(base,  &param->base);
		error |= __get_user(limit, &param->limit);
		error |= __get_user(flags, &param->flags);
		if (error) break;
		/* Is this the right capability to use ? Everybody seems to use
		 * CAPS_SYS_ADMIN as a default to replace suser().
		 */
	  	error = -EPERM;
		if (!capable(CAP_SYS_ADMIN)) break;
		error = -EBADF;
		if ((flags & VME_USE_RMW) && (file->f_mode != 3)) break;

		/* Check a) that at least one DW bit is set, b) that if it 
		 * is not a PIO/MAP only region, only one of the DW bits is 
		 * set, c) that DW is valid for this AM, d) that no VME_USE 
		 * flag incompatible with AM is set, e) that the range is not 
		 * empty, and f) that the address is within the allowed 
		 * range for this AM !
		 */
		error = -EINVAL;
		if (!(VME_DW_MASK & flags) ||
		    (((flags & VME_USE_DMA) || 
		      !(flags & (VME_USE_PIO|VME_USE_MAP))) && 
		     !(dw2ctl[(flags & VME_DW_MASK)>>VME_DW_SHIFT])) ||
		    (am_bad_dw[(flags & VME_AM_MASK)>>VME_AM_SHIFT] & flags) ||
		    (am_bad_use[(flags & VME_AM_MASK)>>VME_AM_SHIFT] & 
		     flags) ||
		    (limit < base) ||
		    (limit >
		     vas_masks[(am2ctl[(flags & VME_AM_MASK) >> VME_AM_SHIFT]
				&UNIVERSE_VAS_MASK) >> UNIVERSE_VAS_SHIFT]))
		  	break;

		error = -ENXIO;
		/* Read and write are only allowed on persistently mapped
		 * regions. Which might not be frequent in the future.
		 */
		if (flags & (VME_USE_PIO|VME_USE_MAP)) {
			reg = find_root_region(flags, base, limit);
			if (!reg) break; 
			if (flags & VME_USE_PIO) {
			  	if (!(reg->flags & VME_USE_PIO)) break;
				FPD->kvaddr = reg->kvaddr + 
				  (base - reg->base);
			} else {
				FPD->kvaddr = 0;
			}
			flags &= (reg->flags | ~VME_DW_MASK);
			FPD->phyaddr = reg->phyaddr + 
			  (base - reg->base);
		} else {
			FPD->phyaddr = 0;
			FPD->kvaddr = 0;
		}
		/* Set file pointer to 0 ? */
		FPD->flags = flags;
		FPD->base = base;
		FPD->limit = limit;
		error = 0;
	  	break;
	
	case VME_GET_ATTR:
		error = __put_user(FPD->base, &param->base);
		error |= __put_user(FPD->limit, &param->limit);
	  	error |= __put_user(FPD->flags, &param->flags);
	  	break;
	
#undef param
#define param ((VME_safe_access *) arg)
	case VME_WRITE8:
	case VME_WRITE16:
	case VME_WRITE32:
		error = __get_user(value, &param->value);
		if (error) break;
	case VME_READ8:
	case VME_READ16:
	case VME_READ32:
	  	error = __get_user(offset, &param->offset);
		size = 1<<(_IOC_NR(cmd)-_IOC_NR(VME_READ8));
		if (error) break;
		
		/* Check that the access is allowed */ 
		error = -EBADF;
		if (!(file->f_mode & 
		      ((_IOC_DIR(cmd) & _IOC_READ) ? 
		       FMODE_READ : FMODE_WRITE))) break;

		/* Check that the address is valid */
		error = -EINVAL;
		if (((offset+size-1) < offset) ||
		    ((offset+size-1) > (FPD->limit - FPD->base))) break;

		error = vme_safe_access(cmd, FPD->flags, FPD->base+offset, 
					&value);
		if (error || !(_IOC_DIR(cmd) & _IOC_READ)) break;
		error = __put_user(value, &param->value);
		break;
#undef param

#define param ((VME_modbits *) arg)
	case VME_MODBITS8:
	case VME_MODBITS16:
	case VME_MODBITS32:
		error = -EINVAL;
		if (!(FPD->flags & VME_USE_RMW)) break;
	  	error = __get_user(offset, &param->offset);
		error |= __get_user(rmwval, &param->value);
		error |= __get_user(mask, &param->mask);

		size = 1<<(_IOC_NR(cmd)-_IOC_NR(VME_MODBITS8));
		if (error) break;
		error = -EINVAL;
		/* Check that the address is within the allowed range */
		if (((offset+size-1) < offset) ||
		    ((offset+size-1) > (FPD->limit - FPD->base))) break;

		error = vme_modbits(cmd, FPD->flags, 
				    FPD->base + offset, &rmwval, mask);
		if (error) break;
		error = __put_user(rmwval, &param->value);
		break;
#undef param
 
#if 0
		/* Not implemented for now, it would require locking
		 * user mode memory for the duration of the operation
		 * or be sure that the user has locked the memory
		 * (but still translating all addresses to kernel
		 * virtual addresses).
		 */
#define param ((VME_dma *)arg)
	case VME_DMA_READ:
    
	  	break;

	case VME_DMA_WRITE:
    
	  	break;
#undef param
#endif

	default:
	  	error = -EINVAL;
		break;
	}
	return error;
}

/* The interrupt routine is somewhat complex: the device interrupt handler 
 * may wish to start a DMA operation but we want to defer the activation
 * of the DMA until after all interrupts handlers have been called.
 * Otherwise the following interrupt handlers might have to wait for the DMA 
 * throttle to release the bus, which might take up to a few hundred 
 * microseconds, or an eternity for a modern processor. So we handle the 
 * device interrupts after having set a flag prohibiting vme_queue_dmalist 
 * to trigger any operation, than the DMA interrupt if applicable and finally 
 * we start DMA if it is inactive there are pending DMA operations. The flag 
 * also would prohibit DMA operations from being started from another 
 * processor on a SMP system. The first 3 lines are ugly, acquiring and 
 * releasing a spinlock just to set a flag, if you have a cleaner solution 
 * please tell me. 
 */ 

/* I truly wish an ilog2 or something like this would exist, either as 
 * a compiler builtin or in asm/bitops to find the MSB of an unsigned
 * and automatically optimized according to arch and CONFIG options.
 */
#if defined(__powerpc__)
static inline u_int 
find_highest_virq(u_int mask) {
	u_int lz;
	asm("cntlzw %0,%1" : "=r" (lz) : "r" (mask & UNIVERSE_LINT_VIRQS));
	return 31-lz;
}
#else 
/* A special case could be defined for x86, but bsr is __slow__ on Pentia */
static inline u_int
find_highest_virq(u_int mask) {
	u_int lvl=0;
	if (mask&0xf0) { lvl+=4; mask>>=4; }
	if (mask&0x0c) { lvl+=2; mask>>=2; }
	return lvl + ((mask >> 1) & 1);
}
#endif

static void irq_action(int irq, void *dev_id, struct pt_regs *regs) {
	struct private *lp = (struct private *) dev_id;
	volatile u_char *__rp = lp->reg_base;
  	u_int reason;
	int error;
	u32 dgcs;
	struct vme_dma *dma;

	/* Note that this flag can be set without a spinlock held, but
	 * not cleared. In most cases it will prevent the DMA from starting
	 * while it's set. Protecting the setting with a spinlock would only
	 * delay the processing of interrupts when another processor holds
	 * the lock to start a DMA operation.
	 */
	set_bit(IRQ_ACTIVE, &lp->state);

	/* We don't want anybody to touch the interrupt array while we are in
	 * the following loop, so we lock against writers. Note that actually 
	 * the interrupt counters are modified (incremented), but it does not 
	 * matter since this code can only execute on one processor at a time 
	 * and the only other place where they are written to is with the
	 * interrupt-safe write lock held when adding an interrupt entry. 
	 * Besides this, a wrong interrupt count would never be a serious
	 * problem: it is only read by the /proc code and used for statistics.
	 */
	read_lock(&lp->lock);
	while ((reason=GET_REG(LINT_STAT)) & UNIVERSE_LINT_VIRQS) {
		struct vme_interrupt * intr;
		u_int lvl, vec, mask;
		lvl = find_highest_virq(reason);
		vec=GET_REG(V1_STATID-4+4*lvl);
		mask = 1<<lvl;
		if (!(vec & UNIVERSE_STATID_ERR) && 
		    (intr=lp->ints[lvl-1]
		     [vec & UNIVERSE_STATID_VECTOR_MASK])) {
			intr->count++;
			intr->handler(intr, lvl, vec);
		} else {
			if (vec & UNIVERSE_STATID_ERR) {
				vec = UNIVERSE_STATID_ERR;
			}
			/* If the same spurious vector repeats
			 * within 2 jiffies, then there is likely
			 * a problem with a stuck interrupt line,
			 * or an unhandled interrupt which is not
			 * of the ROAK type. 
			 */
			if (lp->virq[lvl-1].errors != 0 && 
			    jiffies-lp->virq[lvl-1].tstamp < 2 &&
			    lp->virq[lvl-1].vector == vec) {
				lp->valid_virqs &= ~mask;
				lp->cached_lint_en &= ~mask;
				SET_REG(lp->cached_lint_en, LINT_EN);
				printk(KERN_CRIT 
				       "Unhandled VME interrupt "
				       "vector %d on level %d: level "
				       "permanently disabled.\n",
				       vec, lvl);
				/* Here we should find a way 
				 * to call all handlers on this level
				 * to tell them  there is a problem.
				 * That's why the printk is still
				 * here (and not a dprintk).
				 */
				lp->virq[lvl-1].errors++;
			} else {
				printk(KERN_WARNING
				       "Unhandled VME interrupt "
				       "vector %d on level %d!\n",
				       vec, lvl);
				lp->virq[lvl-1].unhandled++;
			}
			lp->virq[lvl-1].tstamp = jiffies;
			lp->virq[lvl-1].vector = vec;
		}
		SET_REG(mask, LINT_STAT);
	}
	read_unlock(&lp->lock);

	dma = NULL;
	if (reason & UNIVERSE_LINT_DMA) {

		spin_lock(&lp->dma_lock);
		dma = lp->dma_queue;
		lp->dma_queue = dma->queue;
		if (lp->dma_queue == NULL) lp->dma_queue_end = NULL;
		lp->state = IRQ_ACTIVE_MASK;
		spin_unlock(&lp->dma_lock);

		del_timer(&lp->dma_timer);
		dgcs = GET_REG(DGCS);
		/* Clear all pending interrupt flags and the LINT_STAT
		 * which must also be cleared.
		 */
		SET_REG(dgcs, DGCS);
		SET_REG(UNIVERSE_LINT_DMA, LINT_STAT);

		if (dgcs & UNIVERSE_DGCS_DONE) {
			error = 0;
			dma->remlen = 0;
		} else if (dgcs & (UNIVERSE_DGCS_LERR | UNIVERSE_DGCS_VERR | 
				   UNIVERSE_DGCS_P_ERR)) {
			error = -EIO;
			/* Should we try to compute here the
			 * the remaining transfer count.
			 * It is the purpose of the remlen field
			 * but it's quite complex, not very useful,
			 * and left as an exercise. The printk
			 * later is often informative enough since
			 * this should only happen when debugging.
			 */
		} else {
			/* This is necessarily DGCS_STOPPED since we don't use
			 * the DMA halt capability. The only posible cause
			 * right now is timeout, but this could change
			 * if DMA priority queues are implemented.
			 */
			error = -ETIME;
		}

	}

	/* The handler has to be called now with the lock free so that it 
	 * can queue further DMA operations. Queued DMA operations will not
	 * be started until all the interrupts have been handled to limit 
	 * conflicts on VME accesses.
	 */
	if (dma) {
		if (error) {
			printk("DMA transfer error: status %x, "
			       " count=%d, packet @%p.\n", 
			       (dgcs>>8) &0xff, GET_REG(DTBC),
			       bus_to_virt(GET_REG(DCPP)));
		}
		dma->error = error;
	  	dma->handler(dma);
		if (!test_bit(VME_DMA_READY, &dma->flags))
			wake_up(&lp->dma_free_wait);
	}
	spin_lock(&lp->dma_lock);
	clear_bit(IRQ_ACTIVE, &lp->state);
	if (!test_bit(DMA_ACTIVE, &lp->state) && (dma=lp->dma_queue)) {
		/* Start the next DMA operation. */
	  	SET_REG(0, DTBC);
		SET_REG(virt_to_bus(dma->private), DCPP);
		iobarrier_w();
		SET_REG(universe.cached_dgcs | UNIVERSE_DGCS_GO, DGCS);
		lp->dma_timer.expires = jiffies + dma->timeout;
		add_timer(&lp->dma_timer);
		set_bit(DMA_ACTIVE, &lp->state);
	}
	spin_unlock(&lp->dma_lock);
}

/* This routine is invoked by timer_bh, with interrupts enabled */
static void dma_timeout(u_long data) {
	struct private *lp= (struct private *)data;
	volatile u_char *__rp = lp->reg_base;
	spin_lock_irq(&lp->dma_lock);
	/* On UP it is possible that this code get interrupted by the
	 * DMA termination event. This probably means that the timeout
	 * value was too short, but should at least be handled gracefully.
	 * The DMA for which this timeout was intended might just have been
	 * removed from the queue and maybe the following one in the queue
	 * has been started. It's even more complex on SMP, but unless I've
	 * missed a weird sequence of events, this is correctly handled by 
	 * the preceding spinlock and by checking that the DMA is still 
	 * active and that the expiration time is not in the future.
	 */
	if (test_bit(DMA_ACTIVE, &lp->state) && 
	    time_after_eq(jiffies, lp->dma_timer.expires)) {
		SET_REG(universe.cached_dgcs | UNIVERSE_DGCS_STOP_REQ, DGCS);
		/* set_bit(DMA_TIMEDOUT,&lp->state); */
		printk(KERN_NOTICE "Universe VME DMA timeout on device %s!",
		       lp->dma_queue->device->name);
	}
	spin_unlock_irq(&lp->dma_lock);
}

#ifdef CONFIG_PROC_FS
static int
get_vme_dev_info(char *buf, char **start, off_t pos, int count)
{
	struct vme_device *dev = &vme_root;
	off_t at = 0;
	int len, cnt;

	cnt = 0;
	read_lock(&universe.lock);
	while (dev && count > cnt) {
	  	if (dev->fops) {
			len = sprintf(buf, "%4d\t%s\n",
				      dev->minor,
				      dev->name);
		} else {
			len = sprintf(buf, "<none>\t%s\n",
				      dev->name);
		}
		at += len;
		if (at >= pos) {
			if (!*start) {
				*start = buf + (pos - (at - len));
				cnt = at - pos;
			} else
				cnt += len;
			buf += len;
		}
		dev = dev->next;
	}
	read_unlock(&universe.lock);
	return (count > cnt) ? cnt : count;
}

static const char * const am_str[]={
	"A64 MBLT",
	"A64 DATA",
	"A64 PROG",
	"A64 BLT",
	"A64 MBLT  PRIV",
	"A64 DATA  PRIV",
	"A64 PROG  PRIV",
	"A64 BLT   PRIV",
	"A32 MBLT",
	"A32 DATA",
	"A32 PROG",
	"A32 BLT",
	"A32 MBLT  PRIV",
	"A32 DATA  PRIV",
	"A32 PROG  PRIV",
	"A32 BLT   PRIV",
	"USER      0x10",
	"USER      0x11",
	"USER      0x12",
	"USER      0x13",
	"USER      0x14",
	"USER      0x15",
	"USER      0x16",
	"USER      0x17",
	"USER      0x18",
	"USER      0x19",
	"USER      0x1A",
	"USER      0x1B",
	"USER      0x1C",
	"USER      0x1D",
	"USER      0x1E",
	"USER      0x1F",
	"6U   2eBLT",
	"3U   2eBLT",
	[0x22 ... 0x28] = "?",
	"A16",
	[0x2a ... 0x2c] = "?",
	"A16       PRIV",
	"?",
	"CR/CSR",
	[0x30 ... 0x37] = "?",
	"A24 MBLT",
	"A24 DATA",
	"A24 PROG",
	"A24 BLT",
	"A24 MBLT  PRIV",
	"A24 DATA  PRIV",
	"A24 PROG  PRIV",
	"A24 BLT   PRIV"
};

static const char * const dw_str[]={"?", "D08", "D16", "?", "D32", 
				    [5 ... 7] = "?", "D64", [9 ... 15] = "?"};

static const char * const map_str[]={"", ",PRM", ",???", ",DYN"};

static int
get_vme_reg_info(char *buf, char **start, off_t pos, int count)
{
	struct vme_device *dev = &vme_root;
	off_t at = 0;
	int len, cnt;
	char sflags[24];

	strcpy(buf, "Address space\tWidth and flags\tRange\t\t\tDevice\n");
        at = cnt = strlen(buf);
	buf += cnt;

	/* Flags: PRM/DYN,DMA,RMW */
	read_lock(&universe.lock);
	while (dev && count > cnt) {
		struct vme_region *p = dev->regions;
		while(p && count>cnt) {
		  	strcpy(sflags, dw_str[(p->flags&VME_DW_MASK)
					    >>VME_DW_SHIFT]);
			strcpy(sflags+3, map_str[p->flags & 
						(VME_USE_MAP|VME_USE_PIO)]);
			if (p->flags & VME_USE_DMA) 
				strcat(sflags, ",DMA"); 
			if (p->flags & VME_USE_RMW) 
				strcat(sflags, ",RMW");
			if (strlen(sflags)<8) strcat(sflags, "\t");
			len = sprintf(buf, " %-14s %s\t%08lx-%08lx\t%s\n",
				      am_str[(p->flags&VME_AM_MASK)
					    >>VME_AM_SHIFT],
				      sflags,
				      p->base,
				      p->limit,
				      p->device->name);
			at += len;
			if (at >= pos) {
			  	if (!*start) {
				  	*start = buf + (pos - (at - len));
					cnt = at - pos;
				} else
				  	cnt += len;
				buf += len;
			}
			p = p->next;
		}
		dev = dev->next;
	}
	read_unlock(&universe.lock);
	return (count > cnt) ? cnt : count;
}


static int
get_vme_int_info(char *buf, char **start, off_t pos, int count)
{
	struct vme_device *dev = &vme_root;
	off_t at;
	int len, cnt, i;

	strcpy(buf, "Level Vector:      Count\tDevice/Interrupt\n");
        at = cnt = strlen(buf);
	buf += cnt;

	read_lock(&universe.lock);
	while (dev && count > cnt) {
		struct vme_interrupt *p = dev->interrupts;
		while(p && count>cnt) {
			len = sprintf(buf, "%5d%7d:%11ld\t%s/%s\n",
				      p->level,
				      p->vector,
				      p->count,
				      p->device->name,
				      p->name);
			at += len;
			if (at >= pos) {
			  	if (!*start) {
				  	*start = buf + (pos - (at - len));
					cnt = at - pos;
				} else
				  	cnt += len;
				buf += len;
			}
			p = p->next;
		}
		dev = dev->next;
	}
	for (i=1; i<8 && count > cnt; i++) {
	  	if (!(universe.virq[i-1].errors ||
		      universe.virq[i-1].unhandled)) continue;
		len = 0;
	  	if (universe.virq[i-1].errors) {
			len = sprintf(buf, 
				      "%5d  Error:%11ld\tuniverse/iackerror\n",
				      i, universe.virq[i-1].errors);
		}
	  	if (universe.virq[i-1].unhandled) {
			len += sprintf(buf+len, 
				      "%5d  Error:%11ld\tuniverse/unhandled\n",
				      i, universe.virq[i-1].unhandled);
		}
		at += len;
		if (at >= pos) {
		  	if (!*start) {
			  	*start = buf + (pos - (at - len));
				cnt = at - pos;
			} else
			  	cnt += len;
			buf += len;
		}
	}

#if defined(UNIVERSE_DEBUG)
	if (count > cnt) {
		u_char tmpstr[16], *p=tmpstr;
		strcpy(tmpstr, "none");
		for (i=1; i<7; i++) {
		  	if (universe.cached_lint_en & (1<<i)) {
				*p++ = i + '0';
				*p++ = ' ';
				*p = '\0';
			}
		}
		len = sprintf(buf, "Enabled VME interrupts: %s\n", tmpstr);
		
		at += len;
		if (at >= pos) {
		  	if (!*start) {
				*start = buf + (pos - (at - len));
				cnt = at - pos;
			} else
				cnt += len;
			buf += len;
		}
	}
#endif
	read_unlock(&universe.lock);
	return (count > cnt) ? cnt : count;
}

#endif

#ifdef CONFIG_PROC_FS

static struct proc_dir_entry *proc_bus_vme_dir;

static __init void vme_proc_init(void)
{
	proc_bus_vme_dir = proc_mkdir("vme", proc_bus);
	if (!proc_bus_vme_dir)
		return;
	create_proc_info_entry("devices", 0, proc_bus_vme_dir, get_vme_dev_info);
	create_proc_info_entry("regions", 0, proc_bus_vme_dir, get_vme_reg_info);
	create_proc_info_entry("interrupts", 0, proc_bus_vme_dir, get_vme_int_info);
}
#else 
#define vme_proc_init();
#endif

#ifdef MODULE
#define universe_init init_module
#endif

#ifdef MODULE
void cleanup_module(void) {
	volatile u_char *__rp = universe.reg_base;
	struct vme_region *reg;

#ifdef CONFIG_PROC_FS
	remove_proc_entry("devices", proc_bus_vme_dir);
	remove_proc_entry("regions", proc_bus_vme_dir);
	remove_proc_entry("interrupts", proc_bus_vme_dir);
	remove_proc_entry("vme", proc_bus);
#endif
	/* mask all interrupts, disable enabled images (SLSI) */
	SET_REG(0, LINT_EN);
	SET_REG(0, VINT_EN);
	
	dprintk("Pending PCI interrupts: %x.\n", GET_REG(LINT_STAT));

	/* Unmap all mapped areas: note that unmapping the 
	 * SLSI unmaps all 8 regions it covers, and that these are the last 
	 * in the list of root regions, hence the loop break condition. 
	 */
	for (reg=vme_root.regions; reg; reg=reg->next) {
		iounmap((void *)reg->kvaddr);
		if (reg == universe.root_regions+8) break;
	}
	vme_root.regions = NULL;
	free_irq(universe.pci->irq, &universe);
	/* Unmap image 0 and registers */
	iounmap((void *)universe.root_regions[0].kvaddr);
	iounmap((void *)universe.reg_base);

	unregister_chrdev(UNIVERSE_MAJOR, "vme");
}
#endif

int __init universe_init(void)
{
	unsigned short pci_command=PCI_COMMAND_MEMORY|PCI_COMMAND_MASTER;
	u32 tmp;
	int i, rb, error;
	volatile u_char * __rp;

	universe.pci = pci_find_device( PCI_VENDOR_ID_TUNDRA, 
					PCI_DEVICE_ID_TUNDRA_CA91C042,
					NULL);
	if (!universe.pci) return -ENODEV;
	pci_read_config_byte(universe.pci, PCI_REVISION_ID, 
			     &universe.revision);
	printk(KERN_INFO 
	       "Universe VME bridge revision #%d found at bus=%d, dev=%d.\n", 
	       universe.revision, universe.pci->bus->number, 
	       PCI_SLOT(universe.pci->devfn));

	/* It is unfortunate but it is necessary to enable the Universe on
	 * the PCI bus to be able to control all the slave images, which can 
	 * cause temporary conflicts in case of bad programming. This is the 
	 * job of the system firmware/BIOS whatever to ensure that these 
	 * conflicts do not happen.
	 */
	/* FIXME:
	 * It's quite a mess here: it works only on PPC for now if 
	 * the Universe is revision 1 and the base address is mapped 
	 * in I/O space. Anyway this area needs 64kB so it can only 
	 * be mapped in memory space on Intel machines. And for 
	 * revision 2, one base is in I/O space and the other in 
	 * memory so we always chose the memory mapped one. 
	 */
  
#define base universe.pci->resource
	if (universe.revision !=0 && 
	    (base[0].flags & PCI_BASE_ADDRESS_SPACE)==PCI_BASE_ADDRESS_SPACE_IO)
		rb = 1;
	else
		rb = 0;
	
	if ((base[rb].flags & PCI_BASE_ADDRESS_SPACE)==PCI_BASE_ADDRESS_SPACE_IO) {
#ifdef __powerpc__ 
	  	pci_command |= PCI_COMMAND_IO;
		universe.reg_base = (u_char *)_IO_BASE + 
			(base[rb].flags & PCI_BASE_ADDRESS_IO_MASK);
		printk(KERN_NOTICE 
		       "Assuming uniform PCI addressing (non PreP)!\n");
#else
		printk(KERN_ERR "Don't know how to address I/O space.\n");
#endif
	} else {
		pci_read_config_dword(universe.pci, 
				      PCI_BASE_ADDRESS_0 + 4*rb,
				      & tmp);
		universe.bus_delta = base[rb].start - tmp;
		universe.reg_base = 
		  ioremap(base[rb].start, 0x1000);
#undef base
	}
	__rp = universe.reg_base;

	if (!__rp) {
	  	printk(KERN_ERR "Can't access universe registers !\n");
		return -ENOMEM;
	}

	/* Enable the Universe on the PCI bus. */
	pci_write_config_word(universe.pci, PCI_COMMAND, pci_command);
	
	/* Clear the SYSFAIL bit and corresponding interrupt if set */
	if(GET_REG(VCSR_CLR)&UNIVERSE_CSR_SYSFAIL) {
		SET_REG(UNIVERSE_CSR_SYSFAIL, VCSR_CLR);
		SET_REG(UNIVERSE_LINT_SYSFAIL, LINT_STAT);
	}
  
	/* Clear PCI and VME error logs (note that they are only used by
	 * posted write cycles).
	 */

	tmp = GET_REG(LERRLOG);
	if(tmp & UNIVERSE_LERRLOG_VALID){
		printk(KERN_INFO "PCI error log cleared, error CMD=%x , "
		       "multiple= %d, address=%x.\n",
		       (tmp>>UNIVERSE_LERRLOG_CMD_SHIFT)&
		       UNIVERSE_LERRLOG_CMD_MASK,
		       (tmp&UNIVERSE_LERRLOG_MULTIPLE)!=0,
		       GET_REG(LAERR));
		SET_REG(UNIVERSE_LERRLOG_VALID, LERRLOG);
	}

	tmp = GET_REG(VERRLOG);
	if(tmp & UNIVERSE_VERRLOG_VALID){
		printk(KERN_INFO "VME error log cleared, error AM=%d, "
		       "IACK=%d, multiple=%d, address=%x.\n",
		       (tmp>>UNIVERSE_VERRLOG_AM_SHIFT)&
		       UNIVERSE_VERRLOG_AM_MASK,
		       (tmp&UNIVERSE_VERRLOG_IACK)!=0,
		       (tmp&UNIVERSE_VERRLOG_MULTIPLE)!=0,
		       GET_REG(VAERR));
		SET_REG(UNIVERSE_VERRLOG_VALID, VERRLOG); 
	}

	/* Disable all interrupts map all VME interrupts to PCI interrupt 0. */
  
	SET_REG(0, LINT_EN);
	SET_REG(0, VINT_EN);
	SET_REG(0, LINT_MAP0);
	SET_REG(0, LINT_MAP1);
	SET_REG(0, VINT_MAP0);
	SET_REG(0, VINT_MAP1);
	
	/* Clear pending interrupts by writing back_data status onto itself. */
	SET_REG(GET_REG(LINT_STAT), LINT_STAT);
	/* SET_REG(GET_REG(VINT_STAT), VINT_STAT); */

	/* Initialize DMA */
	SET_REG(0, D_LLUE);
	tmp=GET_REG(DGCS);
	if(tmp&UNIVERSE_DGCS_ACT) {
		SET_REG(UNIVERSE_DGCS_STOP_REQ|
			(tmp & (UNIVERSE_DGCS_CHAIN | 
				UNIVERSE_DGCS_VON_MASK |
				UNIVERSE_DGCS_VOFF_MASK)), DGCS);
		printk(KERN_WARNING 
		       "Universe DMA active before initialization!\n");
		/* There should be some timeout here. */
		while(GET_REG(DGCS)&UNIVERSE_DGCS_ACT){
			udelay(10); /* Should wait a few us */
		}
	}

	universe_setup(__rp);

	universe.cached_dgcs = GET_REG(DGCS);

	/* Now we analyze the contents of all the image registers and look
	 * for the enabled ones and store their characteristic in the
	 * root_regions array (4 for Universe I, 8 for Universe II), 
	 * but first handle the special slave image.
	 */
#define reg universe.root_regions
	do {
		u32 flags = 0, slsi = GET_REG(SLSI);
		u_long paddr, vaddr=0; 
		if (!(slsi & UNIVERSE_SLAVE_EN)) break;
		if ((slsi&UNIVERSE_SLAVE_LAS_MASK) != UNIVERSE_SLAVE_LAS_MEM) {
			printk(KERN_ERR "Special slave image cannot be "
			       "memory mapped (LAS=%x)!", 
			       slsi&UNIVERSE_SLAVE_LAS_MASK);
			break;
		}

		paddr = (((slsi & UNIVERSE_SLSI_BS_MASK)
				       << UNIVERSE_SLSI_BS_ADDR_SHIFT)
				      + universe.bus_delta);
		if (permanent_maps & 1) {

			vaddr = (u_long) ioremap(paddr, 0x4000000);
			if(!vaddr) {
			  	printk(KERN_WARNING 
				       "Can't ioremap() special slave "
				       "image: lack of free virtual kernel "
				       "space ?\n");
			}
			else {
				flags = VME_USE_PIO;
			}
		}

		/* Register backwards because it's easier to insert at head */
		for (i=15; i>=8; i--) {
			u_long attr = slsi>>((i&7)>>1);
			u_long delta = ((i&6) << 23) + (i&1) * 0xff0000;
			reg[i].phyaddr = paddr + delta;
			reg[i].kvaddr = vaddr ? 
			  ((volatile u_char *) (vaddr + delta)) : NULL;
			reg[i].flags = flags | slsi2am[i&1]
				[(attr>>UNIVERSE_SLSI_SPACE_SHIFT)&1]
				[(attr>>UNIVERSE_SLSI_PRIV_SHIFT)&1] |
				VME_DW(16<<((attr&UNIVERSE_SLSI_VDW_32)
					    >>UNIVERSE_SLSI_VDW_SHIFT));
			reg[i].next = vme_root.regions;
			vme_root.regions = reg+i;
		}
	} while(0);

	/* Note: image 0 is reserved for atomic and safe accesses. */
	for (i=universe.revision ? 7:3; i>0; i--) {
		u32 pcibase, pcilimit, offset, ctl, base, limit, vmask, flags;
		const struct ctl2am_t * p;
		ctl = GET_REG(LSI_CTL(i));
		if (!(ctl&UNIVERSE_SLAVE_EN)) continue;
		pcibase = GET_REG(LSI_BS(i));
		pcilimit = GET_REG(LSI_BD(i)) - 1;
		offset =  GET_REG(LSI_TO(i));

		if (pcilimit < pcibase) {
			printk(KERN_NOTICE
			       "Local slave image %d is enabled but "
			       "its address range (%x:%x) is empty!\n",
			       i, pcibase, pcilimit);
			continue;
		}

		if ((ctl&UNIVERSE_SLAVE_LAS_MASK) != UNIVERSE_SLAVE_LAS_MEM) {
			printk(KERN_ERR "Local slave image %d is not in "
			       "memory space (LAS=%x)!\n", i, 
			       (ctl & UNIVERSE_SLAVE_LAS_MASK) 
			       >> UNIVERSE_SLAVE_LAS_SHIFT);
			continue;
		}
		/* Translate the Universe ctl to a more sensible AM value */
		for(p=ctl2am; ((p->value ^ ctl) & p->mask); p++);
		if (p->flags == ~0U) {
			printk(KERN_ERR 
			       "Local slave image %d has an invalid or "
			       "unsupported attribute combination: %x\n",
			       i, ctl);
			continue;
		}
		flags = p->flags | VME_DW(8 << ((ctl&UNIVERSE_VDW_MASK)
						>> UNIVERSE_VDW_SHIFT));
		vmask = vas_masks[(ctl & UNIVERSE_VAS_MASK) >> 
				 UNIVERSE_VAS_SHIFT];
		reg[i].base = base = (pcibase+offset) & vmask; 
		reg[i].limit = limit = base + pcilimit - pcibase;
		if ((limit < base) ||
		    (limit > vmask)) {
			printk(KERN_ERR
			       "Image %d wraps around or exceeds the limit"
			       "of its VME address space: %x, %x, %x\n",
			       i, ctl, base, limit);
			continue;
			
		}
		reg[i].phyaddr = pcibase + universe.bus_delta;
		
		if (permanent_maps & (1<<i)) {
			reg[i].kvaddr = ioremap(reg[i].phyaddr, limit+1-base);
			if(reg[i].kvaddr == NULL) {
			  	printk(KERN_WARNING
				       "Can't ioremap() slave image %d: lack "
				       "of free virtual kernel space ?\n", i);
			} else {
				flags |= VME_USE_PIO;
			};
		}
		reg[i].flags = flags;
		reg[i].next = vme_root.regions;
		vme_root.regions = reg+i;
	}
#undef reg

	do {
		/* We assume that image 0 has been setup as needed by 
		 * this driver: 8 kB somewhere in the PCI memory space.
		 * Note that here the physical address is the address
		 * on the PCI side and not on the processor side if the
		 * host bridge happens to add an offset. It is needed
		 * for the special cycle generator setup. 
		 */
	  	universe.root_regions[0].phyaddr = GET_REG(LSI_BS(0));
		universe.root_regions[0].kvaddr = 
		  ioremap(universe.root_regions[0].phyaddr + 
			  universe.bus_delta, 0x2000);
		error = -ENOMEM;
		if (!universe.root_regions[0].kvaddr) break;
		/* Here we grab the interrupt line, given the number of 
		 * possible reasons for an interrupt in the Universe, all sane
		 * designs should not force it to share it with other devices.
		 */
		error = request_irq(universe.pci->irq, irq_action, 
				    SA_INTERRUPT, vme_root.name, &universe);
		if(error) {
			printk(KERN_ERR
			       "Unable to grab interrupt %d for VME !\n", 
			       universe.pci->irq);
			break;
		}

		error = register_chrdev(UNIVERSE_MAJOR,"vme",&universe_fops);
		if (error) {
			dprintk("can't get Major %d\n", UNIVERSE_MAJOR);
			break;
		}

		/* Enable the interrupts: individual VME IRQ are only enabled 
		 * if at least one handler is attached.
		 */
		SET_REG(universe.cached_lint_en, LINT_EN);

#ifdef UNIVERSE_DEBUG
		print_registers();
#endif
		vme_proc_init();
		return 0;
	} while(0);
#ifdef MODULE
	cleanup_module();
#endif
	return error;
}

