/* Private kernel definitions for Linux VME support.
 * G.Paubert 1997-1999 
 */
#ifndef _VME_VME_H
#define _VME_VME_H

#define VME_MAGIC ('V'+'M'+'E')  /* Should be 0xE8 */

#include <linux/types.h>
#include <linux/ioctl.h>

/* The VME address modifiers in numerical order. Some are still missing,
 * namely A40, MD32, and 2eBLT. The official VME nomenclature is quite 
 * confusing, defining different names for address modifiers and corresponding
 * capabilities, with the following non obvious equivalences:
 * - A16 <-> SHORT
 * - A24 <-> STANDARD
 * - A32 <-> EXTENDED
 * - A64 <-> LONG
 * Only the Ann notation will be employed because it seems much clearer,
 * and the default will be non-privileged data access. 
 */
#define VME_DW_SHIFT			8
#define VME_DW_MASK			0x00000f00
#define VME_DW(x)			(((x)<<(VME_DW_SHIFT-3)) & VME_DW_MASK)

/* Since 2eBLT makes use of 8 bits of extended address modifier codes,
 * we leave room in the flags to implement it in the future.
 */ 

#define VME_AM_MASK			0x3f000000
#define VME_AM_SHIFT			24
#define VME_AM(x)			(((x)<<VME_AM_SHIFT)&VME_AM_MASK)
#define VME_XAM_MASK			0x00ff0000
#define VME_XAM_SHIFT			16

#define VME_AM_A64_MBLT(dws)		(VME_AM(0x00) | VME_DW(dws))
#define VME_AM_A64(dws)      		(VME_AM(0x01) | VME_DW(dws))
#define VME_AM_A64_PROG(dws)		(VME_AM(0x02) | VME_DW(dws))
#define VME_AM_A64_BLT(dws)		(VME_AM(0x03) | VME_DW(dws))

#define VME_AM_A64_PRIV_MBLT(dws)	(VME_AM(0x04) | VME_DW(dws))
#define VME_AM_A64_PRIV(dws)		(VME_AM(0x05) | VME_DW(dws))
#define VME_AM_A64_PRIV_PROG(dws)	(VME_AM(0x06) | VME_DW(dws))
#define VME_AM_A64_PRIV_BLT(dws)	(VME_AM(0x07) | VME_DW(dws))

#define VME_AM_A32_MBLT(dws)		(VME_AM(0x08) | VME_DW(dws))
#define VME_AM_A32(dws)	       		(VME_AM(0x09) | VME_DW(dws))
#define VME_AM_A32_PROG(dws)	       	(VME_AM(0x0a) | VME_DW(dws))
#define VME_AM_A32_BLT(dws)	       	(VME_AM(0x0b) | VME_DW(dws))

#define VME_AM_A32_PRIV_MBLT(dws)	(VME_AM(0x0c) | VME_DW(dws))
#define VME_AM_A32_PRIV(dws)		(VME_AM(0x0d) | VME_DW(dws))
#define VME_AM_A32_PRIV_PROG(dws)	(VME_AM(0x0e) | VME_DW(dws))
#define VME_AM_A32_PRIV_BLT(dws)	(VME_AM(0x0f) | VME_DW(dws))

/* 0x10 to 0x1f are user-defined. */

#define VME_AM_USER(am, dws)		(VME_AM(0x10|(am)) | VME_DW(dws))

/* These are not used for now, the names could still change. The use
 * of the undefined VME_XXXAM macro is deliberate.
 */
#define VME_AM_A32_2eBLT64		(VME_XXXAM(0x20,0x01) | VME_DW(64))
#define VME_AM_A64_2eBLT64		(VME_XXXAM(0x20,0x02) | VME_DW(64))
#define VME_AM_A32_2eBLT32		(VME_XXXAM(0x21,0x01) | VME_DW(32))
#define VME_AM_A40_2eBLT32		(VME_XXXAM(0x21,0x02) | VME_DW(32))

#define VME_AM_A16(dws)			(VME_AM(0x29) | VME_DW(dws)) 
#define VME_AM_A16_PRIV(dws)		(VME_AM(0x2d) | VME_DW(dws))

#define VME_AM_CRCSR(dws)		(VME_AM(0x2f) | VME_DW(dws))

#define VME_AM_A24_MBLT(dws)		(VME_AM(0x38) | VME_DW(dws))
#define VME_AM_A24(dws)			(VME_AM(0x39) | VME_DW(dws))
#define VME_AM_A24_PROG(dws)		(VME_AM(0x3a) | VME_DW(dws))
#define VME_AM_A24_BLT(dws)		(VME_AM(0x3b) | VME_DW(dws))

#define VME_AM_A24_PRIV_MBLT(dws)	(VME_AM(0x3c) | VME_DW(dws))
#define VME_AM_A24_PRIV(dws)		(VME_AM(0x3d) | VME_DW(dws))
#define VME_AM_A24_PRIV_PROG(dws)	(VME_AM(0x3e) | VME_DW(dws))
#define VME_AM_A24_PRIV_BLT(dws)	(VME_AM(0x3f) | VME_DW(dws))

#define VME_DMA_DIRECTION_SHIFT 	31
#define VME_DMA_DIRECTION_MASK		0x80000000
#define VME_DMA_DIRECTION_READ		(0<<VME_DMA_DIRECTION_SHIFT)
#define VME_DMA_DIRECTION_WRITE		(1<<VME_DMA_DIRECTION_SHIFT)

#define VME_DMA(rw, am) \
(VME_DMA_DIRECTION_##rw|VME_AM_##am)

#define VME_USE_SHIFT                  0
#define VME_USE_MASK	               0x0000000f
/* The meaning of these flags changes slightly between the kernel and 
 * application interfaces (there is no confusion since one is kept
 * in the private file area of open files, while the other is exclusively 
 * used by board specific drivers):
 * - PIO: 
 *   from application: enables the use of read() and write() system calls,
 *        used in VME_SET_ATTR ioctl to check that the region is 
 *        permanently mapped
 *   from kernel: requests mapping of the region (vme_register_region),
 * - MAP:
 *   from application: enables the use of mmap() system call, used in
 *        VME_SET_ATTR ioctl to check that the region is accessible
 *        through one of the slave images
 *   from kernel: internal flag used to know when to do ioremap/iounmap
 * - DMA:
 *   from application: not used for now,
 *   from kernel: only used for information through the /proc files
 * - RMW:
 *   from application: enables the VME_RMW ioctls
 *   from kernel: used to validate parameters to vme_register_region, but 
 *                not checked in atomic accesses (vme_modbits)
 */
#define VME_USE_PIO	               1	/* Allows read()/write() */
#define VME_USE_MAP		       2	/* Allows mmap() */
#define VME_USE_DMA	               4	/* Not used for now */
#define VME_USE_RMW	               8	/* Allows atomic ioctls */

/* Flag bits definition layout summary.
 * Bit mask:		Usage:
 * 0x80000000 		DMA direction
 * 0x3f000000           AM code
 * 0x00ff0000           Reserved for extended AM code
 * 0x00000f00           VME bus width
 * 0x0000000f           Required capabilities
 * 0x4000f0f0		Undefined for now
 */

typedef struct {
  u_long base;
  u_long limit;
  u_int  flags;
} VME_attr;

typedef struct {
  u_long offset;
  u_long value;
} VME_safe_access;

/* Atomic accesses can only be 8, 16 or 32 bit wide according to the
 * VME specification. So value and mask are u_int.
 */
typedef struct {
  u_long offset;
  u_int value;
  u_int mask;
} VME_modbits;

/* DMA directly to user space buffer is waiting for a clean and stable
 * kernel interface to enable it.
 */
   
#if 0
typedef struct {
  u_long vme_addr;
  u_long locaddr;
  u_int  length;
  u_int  flags;
} VME_dma;
#endif

#define VME_SET_ATTR		_IOW(VME_MAGIC, 0, VME_attr)
#define VME_GET_ATTR		_IOR(VME_MAGIC, 0, VME_attr)

#define VME_READ8               _IOWR(VME_MAGIC, 1, VME_safe_access)
#define VME_WRITE8              _IOW(VME_MAGIC, 1, VME_safe_access)

#define VME_READ16              _IOWR(VME_MAGIC, 2, VME_safe_access)
#define VME_WRITE16             _IOW(VME_MAGIC, 2, VME_safe_access)

#define VME_READ32              _IOWR(VME_MAGIC, 3, VME_safe_access)
#define VME_WRITE32             _IOW(VME_MAGIC, 3, VME_safe_access)

/* Ioctl number 4 is reserved for 64 bit safe accesses, 
 * which are not implemented right now.
 */

#define VME_MODBITS8            _IOWR(VME_MAGIC, 5, VME_modbits)
#define VME_MODBITS16           _IOWR(VME_MAGIC, 6, VME_modbits)
#define VME_MODBITS32           _IOWR(VME_MAGIC, 7, VME_modbits)

#if 0
#define VME_DMA_READ            _IOWR(VME_MAGIC, 8, VME_dma)
#define VME_DMA_WRITE           _IOWR(VME_MAGIC, 9, VME_dma)
#endif

/* The following is here only to help debugging the DMA functions, it will
 * disappear shortly or move to another include file.
 */
#define CM_MEM20_READP	_IO(VME_MAGIC, 0xc0)
#define CM_MEM20_WRITEP	_IO(VME_MAGIC, 0xc1)

/* I'm not happy with the current read[bwl]/write[bwl] macros because they 
 * always add an eieio after the memory load or store instruction on PPC. 
 * Furthermore there are only little endian versions. 
 * We need something better to allow inserting eieio only where needed.
 */

#if defined(__BIG_ENDIAN__)
/* This assumes that all big endian machines do not need any special
 * machine instructions to access I/O space and might break on some
 * architectures. It should work on 68k and PPC, however.
 */

#define __read_8(addr) (*(volatile __u8 *)(addr)) 
#define __read_be16(addr) (*(volatile __u16 *)(addr)) 
#define __read_be32(addr) (*(volatile __u32 *)(addr)) 

#define __write_8(val, addr) (*(volatile __u8 *)(addr))=val
#define __write_be16(val, addr) (*(volatile __u16 *)(addr))=val
#define __write_be32(val, addr) (*(volatile __u32 *)(addr))=val

#else
/* This is not optimal if the architecture has byte swapping load and store
 * instructions, or if it adds systematically memory synchronization
 * instructions on every read[bwl]/write[bwl]. This seems correct and optimal
 * for Intel and Alpha, not sure about MIPS.
 */

#define __read_8(addr) readb(addr)
#define __read_be16(addr) __be16_to_cpu(readw(addr))
#define __read_be32(addr) __be32_to_cpu(readl(addr)) 

#define __write_8(val, addr) writeb(val, addr)
#define __write_be16(val, addr) writew(__cpu_to_be16(val), addr)
#define __write_be32(val, addr) writel(__cpu_to_be32(val), addr)

#endif

/* Now let us map the preceding macros under the names also used from
 * applications.
 */

#define vme_read8(addr) __read_8(addr)
#define vme_read16(addr) __read_be16(addr)
#define vme_read32(addr) __read_be32(addr)

#define vme_write8(val, addr) __write_8(val, addr)
#define vme_write16(val, addr) __write_be16(val, addr)
#define vme_write32(val, addr) __write_be32(val, addr)

/* The I/O ordering barriers, they are still in a state of flux and all 
 * these architecture dependant definitions are ugly, standard library 
 * should decide on a few names.
 * 
 * Still not happy with the names, suggestions welcome...
 * raw: read after write barrier.
 * waw: write after write barrier, to avoid write combining on archs which 
 *     use explicit barriers and not PTE attributes for this case.
 * rar: read after read barrier, see comment about waw...
 * war: not necessary since I don't know of any processor who would reorder
 *      a write before a read. 
 * Actually only raw is stictly an ordering barrier, the other ones are here
 * to prevent merging (combining/folding/joining or whatever the buzzword for
 * your preferred architecture is) of accesses which may result in a different
 * order on bus, especially if the second access is at a lower address than
 * the first one. 
 * 
 * Some remarks depending on the architecture:
 * - PPC: VME should be mapped guarded and cache inhibited, some applications
 * might run slightly better withh nonguarded accesse but this should never be
 * the default.
 * - x86: VME should be mapped uncacheable, which enforces strong ordering
 * on all accesses. VME accesses using preceding macros are defined
 * as volatile, which the compiler should never reorder and the processor
 * will follow. 
 * - m68k: they use strong ordering or at least can be forced to do so with 
 * suitable PTE attributes it seems. So barriers should not do anything.
 * Some Linux/68k guru should check this, however. 
 *
 * Alternatively barriers might be merged with the vme_{read,write}xx macros,
 * with additional suffixes stipulating whether they must be transformed into
 * as single cycle or prevent merging with the preceding or following access.
 * Thus vme_write16_waw would prevent merging the preceding access, 
 * vme_read32_rar would prevent merging preceingg reads and vme_read8_raw 
 * would prevent the read from passing earlier write. 
 * In this case we might want shorter names vme_readxx->vme_rxx, same for write
 * and the ones with included barriers would be vme_rawxx, vme_rarxx, 
 * and vme_wawxx. 
 */
#if defined(__powerpc__)
#define vme_rar_barrier() __asm__ __volatile__("eieio")
#define vme_raw_barrier() __asm__ __volatile__("eieio")
#define vme_waw_barrier() __asm__ __volatile__("eieio")
#elif defined(__mc68000__) || defined(__i386__)
#define vme_rar_barrier() do { } while(0)
#define vme_raw_barrier() do { } while(0)
#define vme_waw_barrier() do { } while(0)
#else 
#error "VME bus is not supported under Linux for this architecture"
#endif

#include <asm/bitops.h>

struct vme_device {
	struct vme_device *next;
	struct file_operations * fops;
	/* These 3 fields are necessary to perform automatic 
	 * deallocation when unregistering a device.
	 */ 
	struct vme_region *regions;
	struct vme_interrupt *interrupts;
  	struct vme_dma *dmalists;
	char * name;
	void * private;
	u_int minor;
};

struct vme_interrupt {
	struct vme_interrupt * next;
	void (*handler)(struct vme_interrupt *, int, int);
	struct vme_device *device;
	void * handler_data;
	char * name;
	u_long count;
	u_int level, vector;
  	u_int flags;	/* interrupt attributes */
};

struct vme_region {
	struct vme_region * next;
	struct vme_device * device;
	volatile u_char *kvaddr;
	u_long phyaddr;
	u_long base;
	u_long limit;
	u_int flags;
};

struct vme_dma {
  	struct vme_dma * next;	
	struct vme_dma * queue; /* To queue DMA requests */
	struct vme_device * device;
  	void * private;  /* Memory pointer to the head of the list */
	void (*handler)(struct vme_dma *);
	void * handler_data;
  	size_t maxfrags; /* Maximum number of scatter/gather fragments */
	size_t remlen;
	long timeout;	 /* timeout in jiffies */
  	u_int flags; /* VME_DMA_BUSY and other internal flags */
	u32 error; /* Error status: 0, -EIO, -ETIME, ... */
};

/* Set by queue_dmalist and cleared by release_dmalist */
#define VME_DMA_BUSY 0

#define VME_DMA_READY 1

struct vme_dmavec {
	u_long kvaddr;
	u_long vme_addr;
	size_t length;
	u32    flags;
};
#if defined(__powerpc__)
/* These functions have been optimized on PPC, a portable version should
 * be written for other architectures and be exported as a general kernel
 * service. 
 */
extern long copy_user_io(volatile void *, volatile const void*, u_long);
#define copy_io_to_user(d,s,l) copy_user_io(d,s,l)
#define copy_user_to_io(d,s,l) copy_user_io(d,s,l)
#endif


int 
vme_register_device(struct vme_device *);

void 
vme_unregister_device(struct vme_device *);

int
vme_register_region(struct vme_device *, struct vme_region *);

void
vme_unregister_region(struct vme_region *);

int
vme_request_interrupt(struct vme_device *, struct vme_interrupt *);

void
vme_free_interrupt(struct vme_interrupt *);

/* This interface might still change: although it seems pretty stable now. */
int 
vme_alloc_dmalist(struct vme_device *, struct vme_dma *, size_t);

void
vme_free_dmalist(struct vme_dma *);

int
vme_queue_dmalist(struct vme_dma *, struct vme_dmavec *, size_t);

/* This function has to be called in the dma termination handlers. */
extern inline void
vme_release_dmalist(struct vme_dma * dma) {
	clear_bit(VME_DMA_BUSY, &dma->flags);
}

int
vme_safe_access(u_int, u32, u_long, u_long *);

int
vme_modbits(u_int, u32, u_long, u_int *, u_int);

#endif /*!_VME_VME_H*/

