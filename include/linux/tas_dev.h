/*
 * tas_dev.h - MIPS TEST and SET pseudo device interface
 */


#ifndef _TAS_DEV_H
#define _TAS_DEV_H

#define TAS_DEVICE_NAME	"tas"

#ifndef _LANGUAGE_ASSEMBLY

#include <linux/types.h>

struct _tas_area_info {
	__u32 	magic;
	__u32 	pad1;
	void 	*map_addr;
#if _MIPS_SZPTR==32
	__u32 	pad2;
#endif
	};

#endif /*_LANGUAGE_ASSEMBLY*/

#define _TAS_INFO_MAGIC		0x20000306	/* COMPARE_AND_SWAP */

#ifdef __KERNEL__

#include <linux/autoconf.h>
#include <asm/sgidefs.h>

#if !defined (CONFIG_CPU_HAS_LLSC)
#define TAS_NEEDS_EXCEPTION_EPILOGUE	/* MIPS1 or MIPS2 without LL/SC */
#if (_MIPS_ISA == _MIPS_ISA_MIPS1)
#define TAS_NEEDS_RESTART		/* MIPS1 */
#endif 
#endif

#define _TAS_ACCESS_MAGIC	0x00200000
#define _TAS_START_MAGIC	0x00300000

#endif  /* __KERNEL_ */

#endif  /*_TAS_DEV_H */



