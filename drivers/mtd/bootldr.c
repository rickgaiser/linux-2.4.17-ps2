/*
 * Read flash partition table from Compaq Bootloader
 *
 * Copyright 2001 Compaq Computer Corporation.
 *
 * $Id: bootldr.c,v 1.7 2001/11/20 19:31:37 jamey Exp $
 *
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 * COMPAQ COMPUTER CORPORATION MAKES NO WARRANTIES, EXPRESSED OR IMPLIED,
 * AS TO THE USEFULNESS OR CORRECTNESS OF THIS CODE OR ITS
 * FITNESS FOR ANY PARTICULAR PURPOSE.
 *
 */

/*
 * Maintainer: Jamey Hicks (jamey.hicks@compaq.com)
 */

#include <linux/kernel.h>
#include <linux/slab.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <asm/setup.h>
#include <linux/bootmem.h>

enum LFR_FLAGS {
        LFR_SIZE_PREFIX = 1,		/* prefix data with 4-byte size */
        LFR_PATCH_BOOTLDR = 2,	/* patch bootloader's 0th instruction */
        LFR_KERNEL = 4,		/* add BOOTIMG_MAGIC, imgsize and VKERNEL_BASE to head of programmed region (see bootldr.c) */
        LFR_EXPAND = 8               /* expand partition size to fit rest of flash */
};

// the tags are parsed too early to malloc or alloc_bootmem so we'll fix it
// for now

static struct tag_ptable Table;
static struct tag_ptable *partition_table = NULL;
static char cmdline[1024];

/*
 * Format suggested by Nicolas Pitre: mtdpart=<name1>@<size1>:<offset1>[,<name2>@<size2>:<offset2>[,...]]
 */
static int parse_mtdpart(char *cmdline, struct mtd_info *master, struct mtd_partition **pparts)
{
        int npartitions = 0;
        int i;
        int base; 
        int size;
        int last_base = 0;
        int last_size = 0;
        char *start = cmdline;

	struct mtd_partition *parts;
	int namelen = 0;
	char *names; 
        int ret = 0;

        // printk(__FUNCTION__ "{%s}\n", cmdline);
        while (1) {
                char *comma_pos = strchr(start, ',');
                char *at_pos;
                char *colon_pos;
                npartitions++;

                if (comma_pos)
                        *comma_pos = 0;

                at_pos = strchr(start, '@');
                colon_pos = strchr(start, ':');
                if (at_pos) *at_pos = 0;
                if (colon_pos) *at_pos = 0;

                if (*start) {
                        namelen += strlen(start);
                } else {
                        // mtdpartNNN
                        namelen += 12;
                }

                /* restore the separators */
                if (at_pos) *at_pos = '@';
                if (colon_pos) *at_pos = ':';
                if (comma_pos) {
                        start = comma_pos + 1;
                        *comma_pos = ',';
                } else {
                        break;
                }
        }

        if (npartitions == 0) 
                return 0;

        start = cmdline;

        // printk("npartitions=%d\n", npartitions);

	parts = kmalloc(sizeof(*parts)*npartitions + namelen, GFP_KERNEL);
	if (!parts) {
		ret = -ENOMEM;
		goto out;
	}
	names = (char *)&parts[npartitions];
	memset(parts, 0, sizeof(*parts)*npartitions + namelen);

        // now fill in the partition info
	for (i = 0; i < npartitions; i++) {
                char *comma_pos = strchr(start, ',');
                char *at_pos;
                char *colon_pos;
                char *name = start;
                char defaultname[32] = "mtdpartXXX";

                if (comma_pos)
                        *comma_pos = 0;

                at_pos = strchr(start, '@');
                // printk("  at_pos=%s\n", at_pos);
                colon_pos = strchr(start, ':');
                // printk("  colon_pos=%s\n", colon_pos);

                if (at_pos) *at_pos = 0;
                if (colon_pos) *colon_pos = 0;
                if (at_pos == start || colon_pos == start) {
                        // use default name
                        sprintf(defaultname, "mtdpart%d", i);
                        name = defaultname;
                }
                if (at_pos) 
                        base = simple_strtoul(at_pos+1, NULL, 0);
                else
                        base = last_base + last_size;
                if (colon_pos)
                        size = simple_strtoul(colon_pos+1, NULL, 0);
                else
                        size = 0;

		parts[i].name = names;
		names += strlen(name) + 1;
		strcpy(parts[i].name, name);

		parts[i].size = size;
		parts[i].offset = base;
		parts[i].mask_flags = 0;
		
		// printk("        partition %s o=%x s=%x\n", 
		//        parts[i].name, parts[i].offset, parts[i].size);

                last_base = base;
                last_size = size;
                if (comma_pos) {
                        start = comma_pos + 1;
                }
	}

	ret = npartitions;
	*pparts = parts;

 out:

        return ret;
}

int parse_bootldr_partitions(struct mtd_info *master, struct mtd_partition **pparts)
{
	struct mtd_partition *parts;
	int ret = 0;
	int retlen, i;
	int npartitions = 0;
	long partition_table_offset;
	long bootmagic = 0;
	long bootcap = 0;
	int namelen = 0;

	char *names; 

        if (cmdline[0] != 0) {
                return parse_mtdpart(cmdline, master, pparts);
        }

        /* if we did not receive a partition table from the bootldr, then try to read one from flash */
        if (!partition_table) {
                /* verify bootldr magic */
                ret = master->read(master, BOOTLDR_MAGIC_OFFSET, sizeof(long), &retlen, (void *)&bootmagic);
                if (ret) 
                        goto out;
                if (bootmagic != BOOTLDR_MAGIC)
                        goto out;
                /* see if bootldr supports partition tables and where to find the partition table */
                ret = master->read(master, BOOTCAP_OFFSET, sizeof(long), &retlen, (void *)&bootcap);
                if (ret) 
                        goto out;

                if (!(bootcap & BOOTCAP_PARTITIONS))
                        goto out;
                if (bootcap & BOOTCAP_PARAMS_AFTER_BOOTLDR)
                        partition_table_offset = master->erasesize;
                else
                        partition_table_offset = master->size - master->erasesize;

                /* Read the partition table */
                partition_table = (struct tag_ptable *)kmalloc(PAGE_SIZE, GFP_KERNEL);
                if (!partition_table)
                        return -ENOMEM;

                ret = master->read(master, partition_table_offset,
                                   PAGE_SIZE, &retlen, (void *)partition_table);
                if (ret)
                        goto out;
        }

	if (!partition_table)
                return -ENOMEM;
	
	printk(__FUNCTION__ ": magic=%#x\n", partition_table->magic);
	printk(__FUNCTION__ ": numPartitions=%#x\n", partition_table->npartitions);

	/* check for partition table magic number */
	if (partition_table->magic != BOOTLDR_PARTITION_MAGIC) 
		goto out;
	npartitions = (partition_table->npartitions > MAX_NUM_PARTITIONS)?
                MAX_NUM_PARTITIONS:partition_table->npartitions;	

	printk(__FUNCTION__ ": npartitions=%#x\n", npartitions);

	for (i = 0; i < npartitions; i++) {
		namelen += strlen(partition_table->partition[i].name) + 1;
	}

	parts = kmalloc(sizeof(*parts)*npartitions + namelen, GFP_KERNEL);
	if (!parts) {
		ret = -ENOMEM;
		goto out;
	}
	names = (char *)&parts[npartitions];
	memset(parts, 0, sizeof(*parts)*npartitions + namelen);


	// from here we use the partition table
	for (i = 0; i < npartitions; i++) {
                struct bootldr_flash_region *partition = &partition_table->partition[i];
		const char *name = partition->name;
		parts[i].name = names;
		names += strlen(name) + 1;
		strncpy(parts[i].name, name, BOOTLDR_PARTITION_NAMELEN);

		// We rely on the bootloader to do partition expansion
		parts[i].size = partition->size;
		parts[i].offset = partition->base;
		parts[i].mask_flags = 0;
		
		printk("        partition %s o=%x s=%x\n", 
		       parts[i].name, parts[i].offset, parts[i].size);

	}

	ret = npartitions;
	*pparts = parts;

 out:
#if 0
	if (partition_table)
		kfree(partition_table);
#endif
	
	return ret;
}


#if 1
static int __init parse_tag_ptable(const struct tag *tag)
{
#ifdef CONFIG_DEBUG_LL
        int i;
#endif
        struct tag_ptable *ptable = (struct tag_ptable *)(&tag->u);
	int npartitions = ptable->npartitions;

        partition_table = &Table;

#ifdef CONFIG_DEBUG_LL    
        printk("ptable: magic = 0x%x  npartitions= %d \n",
                ptable->magic,ptable->npartitions);
    
        for (i=0; i<ptable->npartitions; i++){
                printk("ptable: partition name = %s base= 0x%x  size= 0x%x flags= 0x%x\n",
                        (char *) (&ptable->partition[i].name[0]),
                        ptable->partition[i].base,
                        ptable->partition[i].size,
                        ptable->partition[i].flags);

        }
#endif

	if ( npartitions > MAX_NUM_PARTITIONS ) {
		printk(__FUNCTION__ ": Too many partitions (%d), trimming to %d\n", 
		       npartitions, MAX_NUM_PARTITIONS);
		npartitions = MAX_NUM_PARTITIONS;
	}

        memcpy((void *)partition_table,(void *) ptable,
               (sizeof(partition_table) + sizeof(struct bootldr_flash_region)*npartitions));
    
        return 0;
}

__tagtable(ATAG_PTABLE_COMPAT, parse_tag_ptable);
#endif

/*
 * Format suggested by Nicolas Pitre: mtdpart=<name1>@<size1>:<offset1>[,<name2>@<size2>:<offset2>[,...]]
 */
static int handle_mtdpart(char *mtdpart)
{
        strcpy(cmdline, mtdpart);
        return 1;
}

#if 0
__setup("mtdpart=", handle_mtdpart);
#endif

static int handle_test(char *mtdpart)
{
        // kmalloc(size, GFP_KERNEL); does not work at this point during boot process
        char *foo;

#ifdef CONFIG_DEBUG_LL
        printascii("calling alloc_pages\n");
#endif
        // foo = alloc_pages(GFP_KERNEL, 0);
#ifdef CONFIG_DEBUG_LL
        printascii("done \n");
#endif
        return 1;
}

__setup("test=", handle_test);

EXPORT_SYMBOL(parse_bootldr_partitions);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Compaq Computer Corporation");
MODULE_DESCRIPTION("Parsing code for Compaq bootldr partitions");
