/*
 *  Mapping for Motorola Falcon/Hawk-based PowerPlus boards 
 *
 *  Copyright 2002 MontaVista Software, Inc.
 *
 *  Derived from a posted patch containing buswidth 8 support
 *  and an MCPN765 mapping file by Jan Rovins <jan.rovins@radisys.com>
 *  Ported to 2.4 and generalized to a complete PowerPlus mapping by
 *  Matt Porter <mporter@mvista.com>
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

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/map.h>
#include <linux/mtd/partitions.h>

/*
 * This mapping relies on support for AMD/Fujitsu flash chips being
 * enabled.  If you choose to enable the 'Specific CFI Flash geometry
 * selection' option, then the CONFIG_MTD_CFI_B8 and CONFIG_MTD_CFI_I4
 * config options must also be set.  These options enable handling of
 * a 64-bit organized flash bank comprised of a pair of 2 x16
 * interleaved chips.
 */

/*
 * Default partitioning is as follows:
 *
 * Partition 1 (Kernel/Initrd): 0x00000000 - 0x001fffff
 * Partition 2 (Filesystem):	0x00200000 - 0x003fffff
 *
 * These defaults are overridden in the initialization
 * process once the actual size of the bank A flash part
 * is determined.
 */

/*
 *  Falcon/Hawk-based PowerPlus boards contain a ROM A
 *  Base/Size Register which provides status and controls
 *  flash bank A. 
 */
#define ROM_A_BASE_SIZE_REG		0xfef80050
#define ROM_A_BASE_SIZE_REG_SIZE	0x00000004

#define ROM_A_BASE_MASK			0xfff00000
#define ROM_A_SIZE_MASK			0x00000007
#define ROM_A_SIZE_RESERVED_MASK	0x00070000
#define ROM_A_SIZE_RESERVED		0x00070000
#define ROM_A_CFG_64			0x00080000
#define ROM_A_WRITE_ENABLE		0x00000001
#define ROM_A_ENABLE			0x00000002

#define ROM_A_PARTITION0_SIZE		0x00200000
#define ROM_A_PARTITION1_SIZE		0x00200000

#define ROM_A_PARTITION0_ADDR		0x00000000
#define ROM_A_PARTITION1_ADDR		(ROM_A_PARTITION0_ADDR | ROM_A_PARTITION0_SIZE)

__u8 powerplus_read8(struct map_info *map, unsigned long ofs)
{
       return __raw_readb(map->map_priv_1 + ofs);
}

__u16 powerplus_read16(struct map_info *map, unsigned long ofs)
{
       return __raw_readw(map->map_priv_1 + ofs);
}

__u32 powerplus_read32(struct map_info *map, unsigned long ofs)
{
       return __raw_readl(map->map_priv_1 + ofs);
}

void powerplus_copy_from(struct map_info *map, void *to, unsigned long from, ssize_t len)
{
       memcpy(to, (void *)(map->map_priv_1  + from), len);
}

void powerplus_write8(struct map_info *map, __u8 d, unsigned long adr)
{
       __raw_writeb(d, map->map_priv_1 + adr);
       mb();
}

void powerplus_write16(struct map_info *map, __u16 d, unsigned long adr)
{
       __raw_writew(d, map->map_priv_1 + adr);
       mb();
}

void powerplus_write32(struct map_info *map, __u32 d, unsigned long adr)
{
       __raw_writel(d, map->map_priv_1 + adr);
       mb();
}

void powerplus_copy_to(struct map_info *map, unsigned long to, const void *from, ssize_t len)
{
       memcpy((void *)(map->map_priv_1  + to), from, len);
}

struct map_info powerplus_map = {
               name: "PowerPlus Bank A flash",
               size: 0,
               buswidth: 8,
               read8: powerplus_read8,
               read16: powerplus_read16,
               read32: powerplus_read32,
               copy_from: powerplus_copy_from,
               write8: powerplus_write8,
               write16: powerplus_write16,
               write32: powerplus_write32,
               copy_to: powerplus_copy_to
};

struct mtd_partition powerplus_parts[] = {
       {
               name    : "Kernel/Initrd",
               offset  : ROM_A_PARTITION0_ADDR,
               size    : ROM_A_PARTITION0_SIZE
       },
       { 
               name    : "Filesystem",
               offset  : ROM_A_PARTITION1_ADDR,
               size    : ROM_A_PARTITION1_SIZE
       }
};

#define PARTITION_COUNT (sizeof(powerplus_parts)/sizeof(struct mtd_partition))

static struct mtd_info *mymtd;

int __init init_powerplus(void)
{ 
	unsigned int rom_a_reg;
	unsigned int rom_a_base;

	powerplus_map.map_priv_2 =
		(unsigned long)ioremap(ROM_A_BASE_SIZE_REG,
				       ROM_A_BASE_SIZE_REG_SIZE);

	if (!powerplus_map.map_priv_2) {
		printk("Failed to ioremap ROM_A_BASE_SIZE_REG\n");
		return -EIO;
	}

	rom_a_reg = __raw_readl(powerplus_map.map_priv_2);
	rom_a_base = (rom_a_reg & ROM_A_BASE_MASK);

	if (!(rom_a_reg & ROM_A_CFG_64)) {
		printk("%s: Only 64-bit flash configuration supported.\n", powerplus_map.name);
		iounmap((void *)powerplus_map.map_priv_2);
		return -EIO;
	}

	if ((rom_a_reg & ROM_A_SIZE_RESERVED_MASK) == ROM_A_SIZE_RESERVED) {
		printk("%s: Illegal ROM A size detected.\n", powerplus_map.name);
		iounmap((void *)powerplus_map.map_priv_2);
		return -EIO;
	}
	else {
		powerplus_map.size = (1 << ((rom_a_reg >> 16) & ROM_A_SIZE_MASK)) * 0x00100000;
	}

	printk("%s: %08x at %08x\n", powerplus_map.name,
			(unsigned int)powerplus_map.size, rom_a_base);

	powerplus_map.map_priv_1 = (unsigned long)ioremap(rom_a_base,
			powerplus_map.size);

	if (!powerplus_map.map_priv_1) {
		printk("Failed to ioremap %08x\n", rom_a_base);
		return -EIO;
	}

	/*
	 * Override default partitioning now that we have
	 * detected the actual size of our ROM A bank. This
	 * will probably be customized for the board's
	 * particular application.  For general purpose use,
	 * we simply split the flash into two equal partitions.
	 */
	powerplus_parts[0].size = powerplus_parts[1].size = powerplus_map.size >> 1;
	powerplus_parts[1].offset = powerplus_parts[0].offset | powerplus_parts[0].size;
	/* 
	 * Enable flash bank for access and writes
	 */
	rom_a_reg = __raw_readl(powerplus_map.map_priv_2);
	rom_a_reg |= ROM_A_ENABLE;
	/*
	 * TBD: need to work out exactly when to turn on & off
	 * the write enable for now it seems OK to leave it on
	 * all the time.
	 */
	rom_a_reg |= ROM_A_WRITE_ENABLE;
	__raw_writel(rom_a_reg, powerplus_map.map_priv_2);

	mymtd = do_map_probe("cfi_probe", &powerplus_map);
	if (mymtd) {
		mymtd->module = THIS_MODULE;
		add_mtd_device(mymtd);
		add_mtd_partitions(mymtd, powerplus_parts, PARTITION_COUNT);
		return 0;
	}
	iounmap((void *)powerplus_map.map_priv_1);
	return -ENXIO;
}

static void __exit cleanup_powerplus(void)
{
	if (mymtd) {
		del_mtd_partitions(mymtd);
		map_destroy(mymtd);
	}
	if (powerplus_map.map_priv_1) {
		iounmap((void *)powerplus_map.map_priv_1);
		powerplus_map.map_priv_1 = 0;
	}
	if (powerplus_map.map_priv_2) {
		iounmap((void *)powerplus_map.map_priv_2);
		powerplus_map.map_priv_2 = 0;
	}
}

module_init(init_powerplus);
module_exit(cleanup_powerplus);

MODULE_AUTHOR("Matt Porter <mporter@mvista.com>");
MODULE_DESCRIPTION("MTD map driver for Motorola Falcon/Hawk-based PowerPlus boards");
MODULE_LICENSE("GPL");
