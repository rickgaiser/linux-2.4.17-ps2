/*
 *  sdmparse.c : sdm partition parser
 *
 *  Copyright 2002 Sony Corporation.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  version 2 of the  License.
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
 *
 * $Id: sdmparse.c,v 1.7.2.1 2002/11/08 11:30:29 takemura Exp $
 */

#include <linux/kernel.h>
#include <linux/malloc.h>
#include <linux/crc32.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/sdmparse.h>

#define SDM_DEV_PREFIX "sdm device"

static struct mtd_info *devices[MAX_SDM_DEV];



static int
check_sdm_table(const struct sdm_table *ptable, u_int32_t sdm_table_size)
{
        u_int32_t        crc;

        if (ptable->magic != SDM_TABLE_MAGIC) {
                SDM_INFO("SDM table invalid magic number 0x%x(0x%x)\n", ptable->magic, SDM_TABLE_MAGIC);
                return SDM_ERR;
        }
        if (ptable->version < SDM_VERSION2) {
                SDM_INFO("SDM table old version\n");
                return SDM_ERR;
        }

        if (ptable->max_descs != SDM_DEFAULT_MAXDESC) {
                SDM_INFO("SDM table has different number of max descriptors %d(%d)\n",
                         ptable->max_descs, SDM_DEFAULT_MAXDESC);
                return SDM_ERR;
        }

        crc = sdm_crc32((u_int8_t *)((u_int32_t)ptable + SDM_TABLE_DESC_OFFSET),
                        sdm_table_size - SDM_TABLE_DESC_OFFSET);
        if (ptable->cksum != crc) {
                SDM_INFO("SDM table invalid cksum 0x%x(0x%x)\n", ptable->cksum, crc);
                return SDM_ERR;
        }
        return SDM_OK;
}

static int
load_sdm_table(struct mtd_info *mtd, struct sdm_table **pptable)
{
        struct sdm_table *ptable;

	u_int32_t sdm_table_size;
        u_int32_t sdm_table_sects;
	size_t retlen;

        sdm_table_size = sizeof(struct sdm_table) 
		+ sizeof(struct sdm_image_desc) * SDM_DEFAULT_MAXDESC;

        sdm_table_sects = (sdm_table_size + mtd->erasesize - 1)/mtd->erasesize;

        if (sdm_table_sects > mtd->size / mtd->erasesize) {
                SDM_INFO("too many max number of desriptors %d\n", SDM_DEFAULT_MAXDESC);
                return SDM_ERR;
        }

        /* allocate sdm_table */
        ptable = (struct sdm_table *)kmalloc(mtd->erasesize * sdm_table_sects, GFP_KERNEL);
        if (ptable == NULL) {
                SDM_INFO("cannot allocate sdm table\n");
                return SDM_ERR;
        }

        /* copy sdm table to work memory */
        if (mtd->read(mtd, SDM_TABLE_STARTSECT * mtd->erasesize, 
		      sdm_table_sects * mtd->erasesize, &retlen, (u_char *)ptable) < 0) {
                return SDM_ERR;
        }

        if (check_sdm_table(ptable, sdm_table_size) != SDM_OK) {
		return SDM_ERR;
	}

	*pptable = ptable;

        return SDM_OK;
}

struct mtd_info *
_lookup_mtd_dev( int devnum )
{
	struct mtd_info *mtd;
	/*
	 * XXX: This sdm partition parser assumes that MTD devices which correspond to
	 *      sdm devices are already registered as MTD dev[0]...dev[SDM_MAX_DEV]
	 */
	if (devices[devnum] == NULL) {
		mtd = __get_mtd_device(NULL, devnum);
		if (strncmp(mtd->name, SDM_DEV_PREFIX, strlen(SDM_DEV_PREFIX)) != 0) {
			printk(KERN_WARNING "sdm device %d not found!\n", devnum);
			return NULL;
		}
		devices[devnum] = mtd;
	}

	return devices[devnum];
};

int
parse_sdm_partitions( void )
{
	struct sdm_table *ptable;
	struct sdm_image_desc *img;
	struct mtd_partition *ppart;
	struct mtd_info *mtd;

	char *pname;
	int num_part, namelen_total = 0;
	int i;

	printk(KERN_INFO "Trying to parse SDM partition table...\n");

	if (load_sdm_table(_lookup_mtd_dev(SDM_MASTER_DEV), &ptable) != SDM_OK) {
		SDM_INFO("SDM table load failed\n");
		return -1;
	}

	img = (struct sdm_image_desc *)((u_int32_t)ptable + SDM_TABLE_DESC_OFFSET);

        for (i = 0, num_part = 0; i < ptable->max_descs; i++) {
                if (img[i].magic == SDM_TABLE_MAGIC) {
			namelen_total += strlen(img[i].name) + 1;
			num_part++;
		}
	}

	ppart = kmalloc(sizeof(struct mtd_partition) * num_part + namelen_total,
			GFP_KERNEL);
	memset(ppart, 0, sizeof(struct mtd_partition) * num_part + namelen_total);
	pname = (char *)&ppart[num_part];

	if (ppart == NULL) {
		SDM_INFO("cannot allocate MTD partition info table\n");
		kfree(ptable);
		return -1;
	}
	
	for (i = 0, num_part = 0; i < ptable->max_descs; i++, img++) {
                if (img->magic == SDM_TABLE_MAGIC) {
			mtd = _lookup_mtd_dev(img->dev);
			ppart[num_part].name = pname;
			ppart[num_part].size = img->sectors * mtd->erasesize;
			ppart[num_part].offset = img->start * mtd->erasesize;
			pname += sprintf(pname, "%s", img->name) + 1;
			add_mtd_partitions(mtd, &ppart[num_part], 1);
			num_part++;
		}
	}

	kfree(ptable);

	printk(KERN_INFO "found %d sdm partition(s)\n", num_part);

	/*
	 * XXX: There's no one who do kfree(ppart). If NOR driver and/or NAND driver
	 *      removed/reloaded, all sdm partitions need to be unregistered once,
	 *      and then re-parsed and registered with add_mtd_partitions.
	 *      If the MTD device corresponding to device 'SDM_MASTER_DEV' is
	 *      unregistered, all sdm partitions need to be unregistered and
	 *      do kfree(ppart).
	 *      Currently, we assume that once NOR driver and NAND driver installed,
	 *      NOR/NAND driver would never removed.
	 */

        return num_part;
}

EXPORT_SYMBOL(parse_sdm_partitions);

#ifdef MODULE
MODULE_AUTHOR("Sony NSC");
MODULE_DESCRIPTION("parse sdm table and translate to mtd partition");
#endif /* MODULE */


