/*
 * linux/include/asm-mips/ps2/dmarelay.h
 *
 *        Copyright (C) 2001  Sony Computer Entertainment Inc.
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License Version 2. See the file "COPYING" in the main
 * directory of this archive for more details.
 *
 */

#ifndef __ASM_PS2_DMARELAY_H
#define __ASM_PS2_DMARELAY_H

#define	SIFNUM_ATA_DMA_BEGIN		0x2000
#define	SIFNUM_GetBufAddr		0
#define	SIFNUM_DmaRead			1
#define	SIFNUM_DmaWrite			2
#define	SIFNUM_ATA_DMA_END		0x2001

/* 0x2002,0x2003,0x2004 and 0x2005 are obsolete. */
#define	SIFNUM_SMAP_TX_DMA_BEGIN	0x2006
#define	SIFNUM_SmapGetTxBufAddr		0
#define	SIFNUM_SmapDmaWrite		1
#define	SIFNUM_SMAP_TX_DMA_END		0x2007
#define	SIFNUM_SMAP_RX_DMA_BEGIN	0x2008
#define	SIFNUM_SmapGetRxBufAddr		0
#define	SIFNUM_SmapDmaRead		1
#define	SIFNUM_SMAP_RX_DMA_END		0x2009

#define ATA_MAX_ENTRIES		256
#define ATA_BUFFER_SIZE		(512 * ATA_MAX_ENTRIES)

struct ata_dma_request {
    int command;
    int size;
    int count;
    int devctrl;
    ps2sif_dmadata_t sdd[ATA_MAX_ENTRIES];
};

#define	SMAP_DMA_ENTRIES		10

struct smap_dma_request {
    int command;
    int size;
    int count;
    int devctrl;
    struct {
	unsigned int i_addr;
	unsigned int f_addr;
	unsigned int size;
	unsigned int sdd_misc;
    } sdd[SMAP_DMA_ENTRIES];
};
#endif /* __ASM_PS2_DMARELAY_H */
