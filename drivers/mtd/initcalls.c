/*
 * $Id: initcalls.c,v 1.3.2.1 2002/11/08 11:30:29 takemura Exp $
 *
 * Init calls for MTD drivers in kernels < 2.2.18
 * Split off into a separate file so they don't pollute
 * the 2.4 and 2.2.18+ versions.
 */
#include <linux/config.h>

extern int init_doc1000(void);
extern int init_doc2000(void);
extern int init_doc2001(void);
extern int init_doc(void);
extern int cfi_probe_init(void);
extern int cfi_intelext_init(void);
extern int cfi_amdstd_init(void);
extern int map_rom_init(void);
extern int map_rom_init(void);
extern int init_physmap(void);
extern int init_rpxlite(void);
extern int init_octagon5066(void);
extern int init_pnc2000(void);
extern int init_vmax301(void);
extern int init_mixmem(void);
extern int init_pmc551(void);
extern int init_nora(void);
extern int init_sbc_mediagx(void);
extern int init_elan_104nc(void);
extern int spia_init(void);
extern int init_mtdram(void);
extern int init_ftl(void);
extern int init_nftl(void);
extern int init_mtdblock(void);
extern int init_mtdchar(void);
extern int init_flagadm(void);

void init_mtd_devices(void)
{
	/* Shedloads of calls to init functions of all the
	 * other drivers and users of MTD, which we can
	 * ditch in 2.2.18 because of the sexy new way of
	 * finding init routines.
	 */
#ifdef CONFIG_MTD_DOC1000
	init_doc1000();
#endif
#ifdef CONFIG_MTD_DOC2000
	init_doc2000();
#endif
#ifdef CONFIG_MTD_DOC2001
	init_doc2001();
#endif
#ifdef CONFIG_MTD_DOCPROBE
	init_doc();
#endif
#ifdef CONFIG_MTD_CFI
	cfi_probe_init();
#endif
#ifdef CONFIG_MTD_CFI_INTELEXT
	cfi_intelext_init();
#endif
#ifdef CONFIG_MTD_CFI_AMDSTD
	cfi_amdstd_init();
#endif
#ifdef CONFIG_MTD_RAM
	map_rom_init();
#endif
#ifdef CONFIG_MTD_ROM
	map_rom_init();
#endif
#ifdef CONFIG_MTD_PHYSMAP
	init_physmap();
#endif
#ifdef CONFIG_MTD_RPXLITE
	init_rpxlite();
#endif
#ifdef CONFIG_MTD_OCTAGON
	init_octagon5066();
#endif
#ifdef CONFIG_MTD_PNC2000
	init_pnc2000();
#endif
#ifdef CONFIG_MTD_VMAX
	init_vmax301();
#endif
#ifdef CONFIGF_MTD_MIXMEM
	init_mixmem();
#endif
#ifdef CONFIG_MTD_PMC551
	init_pmc551();
#endif
#ifdef CONFIG_MTD_NORA
	init_nora();
#endif
#ifdef CONFIG_MTD_SBC_MEDIAGX
	init_sbc_mediagx();
#endif
#ifdef CONFIG_MTD_ELAN_104NC
	init_elan_104nc();
#endif
#ifdef CONFIG_MTD_NAND_SPIA
	spia_init();
#endif
#ifdef CONFIG_MTD_MTDRAM
	init_mtdram();
#endif
#ifdef CONFIG_FTL
	init_ftl();
#endif
#ifdef CONFIG_NFTL
	init_nftl();
#endif
#if defined(CONFIG_MTD_BLOCK) || defined(CONFIG_MTD_BLOCK_RO)
	init_mtdblock();
#endif
#ifdef CONFIG_MTD_CHAR
	init_mtdchar();
#endif
#ifdef CONFIG_MTD_FLAGADM
	init_flagadm();
#endif
}

