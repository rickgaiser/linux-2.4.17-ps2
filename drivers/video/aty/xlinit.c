/*
 *  ATI Rage XL Initialization. Support for Xpert98 and Victoria
 *  PCI cards.
 *
 *  Copyright (C) 2002 MontaVista Software Inc.
 *  Author: MontaVista Software, Inc.
 *         	stevel@mvista.com or source@mvista.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h> 
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/selection.h>
#include <linux/console.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/vt_kern.h>
#include <linux/kd.h> 
#include <asm/io.h>
#include <video/fbcon.h>
#include "mach64.h"
#include "atyfb.h"

#define MPLL_GAIN       0xad
#define VPLL_GAIN       0xd5

#define XPERT98_SUB_ID  0x8008
#define VICTORIA_SUB_ID 0x0000

enum {
	VICTORIA = 0,
	XPERT98,
	NUM_XL_CARDS
};

extern const struct aty_pll_ops aty_pll_ct;

static const struct xl_card_cfg_t {
	int ref_crystal; // 10^4 Hz
	int mem_type;
	int mem_size;
	u32 mem_cntl;
	u32 ext_mem_cntl;
	u32 mem_addr_config;
	u32 bus_cntl;
	u32 dac_cntl;
	u32 hw_debug;
	u32 custom_macro_cntl;
	u8  dll2_cntl;
	u8  pll_yclk_cntl;
} card_cfg[NUM_XL_CARDS] = {
	// VICTORIA
	{	2700, SDRAM, 0x800000,
		0x10757A3B, 0x64000C81, 0x00110202, 0x7b33A040,
		0x82010102, 0x48803800, 0x005E0179,
		0x50, 0x25
	},
	// XPERT98
	{	1432,  WRAM, 0x800000,
		0x00165A2B, 0xE0000CF1, 0x00200213, 0x7333A001,
		0x8000000A, 0x48833800, 0x007F0779,
		0x10, 0x19
	}
};
	  
typedef struct {
	u8 lcd_reg;
	u32 val;
} lcd_tbl_t;

static const lcd_tbl_t lcd_tbl[] = {
	{ 0x01,	0x000520C0 },
	{ 0x08,	0x02000408 },
	{ 0x03,	0x00000F00 },
	{ 0x00,	0x00000000 },
	{ 0x02,	0x00000000 },
	{ 0x04,	0x00000000 },
	{ 0x05,	0x00000000 },
	{ 0x06,	0x00000000 },
	{ 0x33,	0x00000000 },
	{ 0x34,	0x00000000 },
	{ 0x35,	0x00000000 },
	{ 0x36,	0x00000000 },
	{ 0x37,	0x00000000 }
};

static inline u32 aty_ld_lcd(u8 lcd_reg, struct fb_info_aty *info)
{
	aty_st_8(LCD_INDEX, lcd_reg, info);
	return aty_ld_le32(LCD_DATA, info);
}

static inline void aty_st_lcd(u8 lcd_reg, u32 val,
			      struct fb_info_aty *info)
{
	aty_st_8(LCD_INDEX, lcd_reg, info);
	aty_st_le32(LCD_DATA, val, info);
}

static void reset_gui(struct fb_info_aty *info)
{
	aty_st_8(GEN_TEST_CNTL+1, 0x01, info);
	aty_st_8(GEN_TEST_CNTL+1, 0x00, info);
	aty_st_8(GEN_TEST_CNTL+1, 0x02, info);
	mdelay(5);
}


static void reset_sdram(struct fb_info_aty *info)
{
	u8 temp;

	temp = aty_ld_8(EXT_MEM_CNTL, info);
	temp |= 0x02;
	aty_st_8(EXT_MEM_CNTL, temp, info); // MEM_SDRAM_RESET = 1b
	temp |= 0x08;
	aty_st_8(EXT_MEM_CNTL, temp, info); // MEM_CYC_TEST    = 10b
	temp |= 0x0c;
	aty_st_8(EXT_MEM_CNTL, temp, info); // MEM_CYC_TEST    = 11b
	mdelay(5);
	temp &= 0xf3;
	aty_st_8(EXT_MEM_CNTL, temp, info); // MEM_CYC_TEST    = 00b
	temp &= 0xfd;
	aty_st_8(EXT_MEM_CNTL, temp, info); // MEM_SDRAM_REST  = 0b
	mdelay(5);
}

static void init_dll(struct fb_info_aty *info)
{
	// enable DLL
	aty_st_pll(PLL_GEN_CNTL,
		   aty_ld_pll(PLL_GEN_CNTL, info) & 0x7f,
		   info);

	// reset DLL
	aty_st_pll(DLL_CNTL, 0x82, info);
	aty_st_pll(DLL_CNTL, 0xE2, info);
	mdelay(5);
	aty_st_pll(DLL_CNTL, 0x82, info);
	mdelay(6);
}

static void reset_clocks(struct fb_info_aty *info, struct pll_ct *pll,
			 int hsync_enb)
{
	reset_gui(info);
	aty_st_pll(MCLK_FB_DIV, pll->mclk_fb_div, info);
	aty_st_pll(SCLK_FB_DIV, pll->sclk_fb_div, info);

	mdelay(15);
	init_dll(info);
	aty_st_8(GEN_TEST_CNTL+1, 0x00, info);
	mdelay(5);
	aty_st_8(CRTC_GEN_CNTL+3, 0x04, info);
	mdelay(6);
	reset_sdram(info);
	aty_st_8(CRTC_GEN_CNTL+3,
		 hsync_enb ? 0x00 : 0x04, info);

	aty_st_pll(SPLL_CNTL2, pll->spll_cntl2, info);
	aty_st_pll(PLL_GEN_CNTL, pll->pll_gen_cntl, info);
	aty_st_pll(PLL_VCLK_CNTL, pll->pll_vclk_cntl, info);
}


int atyfb_xl_init(struct fb_info_aty *info)
{
	int i, err, xl_card;
	u32 temp;
	u16 temp16;
	union aty_pll pll;
	const struct xl_card_cfg_t * card;

	pci_read_config_word(info->pdev, PCI_SUBSYSTEM_ID, &temp16);
	switch (temp16) {
	case XPERT98_SUB_ID:
		xl_card = XPERT98;
		break;
	case VICTORIA_SUB_ID:
		xl_card = VICTORIA;
		break;
	default:
		printk("\n" __FUNCTION__ ": unknown adapter id 0x%04x\n",
		       temp16);
		return -ENODEV;
	}
	
	card = &card_cfg[xl_card];

	aty_st_8(CONFIG_STAT0, 0x85, info);
	mdelay(10);

	/*
	 * The following needs to be set before the call
	 * to var_to_pll() below. They'll be re-set again
	 * to the same values in aty_init().
	 */
	info->ref_clk_per = 100000000UL/card->ref_crystal;
	info->ram_type = card->mem_type;
	info->total_vram = card->mem_size;
	if (xl_card == VICTORIA) {
		// the MCLK, XCLK are 120MHz on victoria card
		info->mclk_per = 1000000/120;
		info->xclk_per = 1000000/120;
		info->features &= ~M64F_MFB_TIMES_4;
	}

	/*
	 * Calculate mclk and xclk dividers, etc. The passed
	 * pixclock and bpp values don't matter yet, the vclk
	 * isn't programmed until later.
	 */
	if ((err = aty_pll_ct.var_to_pll(info, 39726, 8, &pll)))
		return err;

	aty_st_pll(LVDS_CNTL0, 0x00, info);
	aty_st_pll(DLL2_CNTL, card->dll2_cntl, info);
	aty_st_pll(V2PLL_CNTL, 0x10, info);
	aty_st_pll(MPLL_CNTL, MPLL_GAIN, info);
	aty_st_pll(VPLL_CNTL, VPLL_GAIN, info);
	aty_st_pll(PLL_VCLK_CNTL, 0x00, info);
	aty_st_pll(VFC_CNTL, 0x1B, info);
	aty_st_pll(PLL_REF_DIV, pll.ct.pll_ref_div, info);
	aty_st_pll(PLL_EXT_CNTL, pll.ct.pll_ext_cntl, info);
	aty_st_pll(SPLL_CNTL2, 0x03, info);
	aty_st_pll(PLL_GEN_CNTL, 0x44, info);

	reset_clocks(info, &pll.ct, 0);
	mdelay(10);

	aty_st_pll(VCLK_POST_DIV, 0x03, info);
	aty_st_pll(VCLK0_FB_DIV, 0xDA, info);
	aty_st_pll(VCLK_POST_DIV, 0x0F, info);
	aty_st_pll(VCLK1_FB_DIV, 0xF5, info);
	aty_st_pll(VCLK_POST_DIV, 0x3F, info);
	aty_st_pll(PLL_EXT_CNTL, 0x40 | pll.ct.pll_ext_cntl, info);
	aty_st_pll(VCLK2_FB_DIV, 0x00, info);
	aty_st_pll(VCLK_POST_DIV, 0xFF, info);
	aty_st_pll(PLL_EXT_CNTL, 0xC0 | pll.ct.pll_ext_cntl, info);
	aty_st_pll(VCLK3_FB_DIV, 0x00, info);

	aty_st_8(BUS_CNTL, 0x01, info);
	aty_st_le32(BUS_CNTL, card->bus_cntl | 0x08000000, info);

	aty_st_le32(CRTC_GEN_CNTL, 0x04000200, info);
	aty_st_le16(CONFIG_STAT0, 0x0020, info);
	aty_st_le32(MEM_CNTL, 0x10151A33, info);
	aty_st_le32(EXT_MEM_CNTL, 0xE0000C01, info);
	aty_st_le16(CRTC_GEN_CNTL+2, 0x0000, info);
	aty_st_le32(DAC_CNTL, card->dac_cntl, info);
	aty_st_le16(GEN_TEST_CNTL, 0x0100, info);
	aty_st_le32(CUSTOM_MACRO_CNTL, 0x003C0171, info);
	aty_st_le32(MEM_BUF_CNTL, 0x00382848, info);

	aty_st_le32(HW_DEBUG, card->hw_debug, info);
	aty_st_le16(MEM_ADDR_CONFIG, 0x0000, info);
	aty_st_le16(GP_IO+2, 0x0000, info);
	aty_st_le16(GEN_TEST_CNTL, 0x0000, info);
	aty_st_le16(EXT_DAC_REGS+2, 0x0000, info);
	aty_st_le32(CRTC_INT_CNTL, 0x00000000, info);
	aty_st_le32(TIMER_CONFIG, 0x00000000, info);
	aty_st_le32(0xEC, 0x00000000, info);
	aty_st_le32(0xFC, 0x00000000, info);

	for (i=0; i<sizeof(lcd_tbl)/sizeof(lcd_tbl_t); i++) {
		aty_st_lcd(lcd_tbl[i].lcd_reg, lcd_tbl[i].val, info);
	}

	aty_st_le16(CONFIG_STAT0, 0x00A4, info);
	mdelay(10);

	aty_st_8(BUS_CNTL+1, 0xA0, info);
	mdelay(10);
	
	reset_clocks(info, &pll.ct, 1);
	mdelay(10);

	// something about power management
	aty_st_8(LCD_INDEX, 0x08, info);
	aty_st_8(LCD_DATA, 0x0A, info);
	aty_st_8(LCD_INDEX, 0x08, info);
	aty_st_8(LCD_DATA+3, 0x02, info);
	aty_st_8(LCD_INDEX, 0x08, info);
	aty_st_8(LCD_DATA, 0x0B, info);
	mdelay(2);
	
	// enable display requests, enable CRTC
	aty_st_8(CRTC_GEN_CNTL+3, 0x02, info);
	// disable display
	aty_st_8(CRTC_GEN_CNTL, 0x40, info);
	// disable display requests, disable CRTC
	aty_st_8(CRTC_GEN_CNTL+3, 0x04, info);
	mdelay(10);

	aty_st_pll(PLL_YCLK_CNTL, 0x25, info);

	aty_st_le16(CUSTOM_MACRO_CNTL, 0x0179, info);
	aty_st_le16(CUSTOM_MACRO_CNTL+2, 0x005E, info);
	aty_st_le16(CUSTOM_MACRO_CNTL+2, card->custom_macro_cntl>>16, info);
	aty_st_8(CUSTOM_MACRO_CNTL+1,
		 (card->custom_macro_cntl>>8) & 0xff, info);

	aty_st_le32(MEM_ADDR_CONFIG, card->mem_addr_config, info);
	aty_st_le32(MEM_CNTL, card->mem_cntl, info);
	aty_st_le32(EXT_MEM_CNTL, card->ext_mem_cntl, info);

	aty_st_8(CONFIG_STAT0, 0xA0 | card->mem_type, info);

	aty_st_pll(PLL_YCLK_CNTL, 0x01, info);
	mdelay(15);
	aty_st_pll(PLL_YCLK_CNTL, card->pll_yclk_cntl, info);
	mdelay(1);
	
	reset_clocks(info, &pll.ct, 0);
	mdelay(50);
	reset_clocks(info, &pll.ct, 0);
	mdelay(50);

	// enable extended register block
	aty_st_8(BUS_CNTL+3, 0x7B, info);
	mdelay(1);
	// disable extended register block
	aty_st_8(BUS_CNTL+3, 0x73, info);

	aty_st_8(CONFIG_STAT0, 0x80 | card->mem_type, info);

	// disable display requests, disable CRTC
	aty_st_8(CRTC_GEN_CNTL+3, 0x04, info);
	// disable mapping registers in VGA aperture
	aty_st_8(CONFIG_CNTL, aty_ld_8(CONFIG_CNTL, info) & ~0x04, info);
	mdelay(50);
	// enable display requests, enable CRTC
	aty_st_8(CRTC_GEN_CNTL+3, 0x02, info);

	// make GPIO's 14,15,16 all inputs
	aty_st_8(LCD_INDEX, 0x07, info);
	aty_st_8(LCD_DATA+3, 0x00, info);

	// enable the display
	aty_st_8(CRTC_GEN_CNTL, 0x00, info);
	mdelay(17);
	// reset the memory controller
	aty_st_8(GEN_TEST_CNTL+1, 0x02, info);
	mdelay(15);
	aty_st_8(GEN_TEST_CNTL+1, 0x00, info);
	mdelay(30);

	// enable extended register block
	aty_st_8(BUS_CNTL+3,
		 (u8)(aty_ld_8(BUS_CNTL+3, info) | 0x08),
		 info);
	// set FIFO size to 512 (PIO)
	aty_st_le32(GUI_CNTL,
		    aty_ld_le32(GUI_CNTL, info) & ~0x3,
		    info);

	// enable CRT and disable lcd
	aty_st_8(LCD_INDEX, 0x01, info);
	temp = aty_ld_le32(LCD_DATA, info);
	temp = (temp | 0x01) & ~0x02;
	aty_st_le32(LCD_DATA, temp, info);

	return 0;
}

