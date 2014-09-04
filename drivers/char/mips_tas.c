/*
 * mips_tas.c - Common test-and-set(compare_and_swap) interface for MIPS
 *
 *        Copyright (C) 2000, 2001, 2002  Sony Computer Entertainment Inc.
 *        Copyright 2001, 2002  Sony Corp.
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License Version 2. See the file "COPYING" in the main
 * directory of this archive for more details.
 *
 */

#include <linux/autoconf.h>

#ifndef CONFIG_MIPS
#error "Sorry, this device is for MIPS only."
#endif

/*
 * 	Setup/Clean up Driver Module
 */

#ifdef MODULE

#ifndef EXPORT_SYMTAB
#define EXPORT_SYMTAB
#endif

#if defined(CONFIG_MODVERSIONS) && !defined(MODVERSIONS)
#define MODVERSIONS
#endif

#ifdef MODVERSIONS
#include <linux/modversions.h>
#endif

#endif /* MODULE */

#include <linux/init.h>

#include <linux/errno.h>	/* error codes */
#include <linux/kernel.h>	/* printk() */
#include <linux/fs.h>		/* file op. */
#include <linux/proc_fs.h>	/* proc fs file op. */
#include <linux/mman.h>
#include <linux/pagemap.h>
#include <asm/io.h>
#include <asm/uaccess.h>	/* copy to/from user space */
#include <asm/page.h>		/* page size */
#include <asm/pgtable.h>	/* PAGE_READONLY */

#include <linux/tas_dev.h>

#include <linux/module.h>

#include <linux/major.h>

static int tas_major=TASDEV_MAJOR;	

EXPORT_SYMBOL(tas_major);	/* export symbole */
MODULE_PARM(tas_major,"i");	/* as parameter on loaing */


/*
 * File Operations table
 *	please refer <linux/fs.h> for other methods.
 */

static struct file_operations  tas_fops; 


#ifndef TAS_DEVICE_NAME
#define  TAS_DEVICE_NAME "tas"
#endif 


static mem_map_t * tas_code_buffer = 0 ;

#ifdef CONFIG_CPU_HAS_LLSC

/* In this case, CPU has LL/SC. */

/********************
#include<asm/regdef.h>

        .set noreorder
//compare_and_swap:
1:
	ll	t0, (a0)
	move	v0, zero
	bne	t0, a1, 2f
	 nop
	move	v0, a2
	sc	v0, (a0)
	beqz	v0, 1b
	 nop
2:
        j       ra
	 nop
*********************/

static const __u32 tas_code[] = {
/*   0:*/   0xc0880000,		//ll      $t0,0($a0)
/*   4:*/   0x00001021,		//move    $v0,$zero
/*   8:*/   0x15050005,		//bne     $t0,$a1,0x20
/*   c:*/   0x00000000,		// nop
/*  10:*/   0x00c01021,		//move    $v0,$a2
/*  14:*/   0xe0820000,		//sc      $v0,0($a0)
/*  18:*/   0x1040fff9,		//beqz    $v0,0x0
/*  1c:*/   0x00000000,		// nop
/*  20:*/   0x03e00008,		//jr      $ra
/*  24:*/   0x00000000,		//nop

			0};

#else /* CONFIG_CPU_HAS_LLSC */

#ifdef TAS_NEEDS_RESTART

/* In this case, CPU has no branch-likely insn, no LL/SC. */

/********************
#include<asm/regdef.h>

	.set	noreorder
// compare_and_swap:
1:
	move	k1, a0;
	lw	t0, (a0);
	 move	v0, zero;
	bne	t0, a1, 2f
	 nop

	bne	k1, a0, 1b
	 li	v0, 1
	sw	a2, (k1)
2:
	j	ra
	 nop
*********************/

static const __u32 tas_code[] = {
/*   0:*/   0x0080d821,        //move    $k1,$a0
/*   4:*/   0x8c880000,        //lw      $t0,0($a0)
/*   8:*/   0x00001021,        //move    $v0,$zero
/*   c:*/   0x15050004,        //bne     $t0,$a1,0x20
/*  10:*/   0x00000000,        // nop
/*  14:*/   0x1764fffa,        //bne     $k1,$a0,0x0
/*  18:*/   0x24020001,        // li      $v0,1
/*  1c:*/   0xaf660000,        //sw      $a2,0($k1)
/*  20:*/   0x03e00008,        //jr      $ra
/*  24:*/   0x00000000,        //nop
			0};

#else /* TAS_DEV_NEED_RESTART */

/* In this case, CPU has branch-likely insn, but no LL/SC. */
/* We assume CPU has load-blocking. */

/********************
#include<asm/regdef.h>

        .set noreorder
//compare_and_swap:
1:
	move	k1, zero
	lw	t0, (a0)
	move	v0, zero
	bne	t0, a1, 2f
	 nop
	li	v0, 1
	beqzl	k1, 2f
	 sw	a2, (a0)
	b	1b
	 nop
2:
	jr	ra
	 nop
*********************/

static const __u32 tas_code[] = {
/*   0:*/   0x0000d821,		//move	$k1,$zero
/*   4:*/   0x8c880000,		//lw	$t0,0($a0)
/*   8:*/   0x00001021,		//move	$v0,$zero
/*   c:*/   0x15050006,		//bne	$t0,$a1,0x28
/*  10:*/   0x00000000,		// nop
/*  14:*/   0x24020001,		//li	$v0,1
/*  18:*/   0x53600003,		//beqzl	$k1,0x28
/*  1c:*/   0xac860000,		// sw	$a2,0($a0)
/*  20:*/   0x1000fff7,		//b	0x0
/*  24:*/   0x00000000,		// nop
/*  28:*/   0x03e00008,		//jr	$ra
/*  2c:*/   0x00000000,		//nop
			0};

#endif /* TAS_DEV_NEED_RESTART */

#endif /* CONFIG_CPU_HAS_LLSC */


EXPORT_SYMBOL(tas_code_buffer);	/* export symbole */

static spinlock_t lock = {0,};

static 
int try_init_code_buffer(void)
{

	mem_map_t *pg;
	int retval = 0;

	if (!tas_code_buffer) {
		pg = alloc_page(GFP_KERNEL);
		spin_lock(&lock);
		if (!tas_code_buffer) {
			if (!pg) {
				spin_unlock(&lock);
				retval =  -EBUSY;
				goto out;
			}
			tas_code_buffer = pg;
			spin_unlock(&lock);
		} else {
			/* Another thread sets TAS_CODE_BUFFER, while 
			   on calling ALLOC_PAGE() */
			spin_unlock(&lock);
			if (pg) put_page(pg);
			goto out;
		}


		clear_page(page_address(tas_code_buffer));

		memcpy (page_address(tas_code_buffer), (void *)tas_code, 
			(sizeof (tas_code) * sizeof (tas_code[0])));

	}
out:
	return retval;
}

#ifdef MODULE
static 
void try_free_code_buffer(void)
{

	spin_lock(&lock);
	if (tas_code_buffer) {
		mem_map_t *pg = tas_code_buffer;
		tas_code_buffer=0;
		spin_unlock(&lock);
		put_page (pg);
	} else {
		spin_unlock(&lock);
	}
}
#endif

static get_info_t get_tas_info;


/*
 * Caller of (*get_info)() is  proc_file_read() in fs/proc/generic.c
 */
static 
int get_tas_info (char *buf, 	/*  allocated area for info */
	       char **start, 	/*  return youown area if you allocate */
	       off_t pos,	/*  pos arg of vfs read */
	       int count)	/*  readable bytes */
{

/* SPRINTF does not exist in the kernel */
#define MY_BUFSIZE 256
#define MARGIN 16
	char mybuf[MY_BUFSIZE+MARGIN];

	int len;

	len = sprintf(mybuf,
		      "_TAS_INFO_MAGIC:\t0x%8.8x\n"
		      "_TAS_START_MAGIC:\t0x%8.8x\n"
		      "_TAS_ACCESS_MAGIC:\t0x%8.8x\n"
		      "TAS_NEEDS_EXCEPTION_EPILOGUE: %s\n"
		      "TAS_NEEDS_RESTART: %s\n",

		      _TAS_INFO_MAGIC,
		      _TAS_START_MAGIC,
		      _TAS_ACCESS_MAGIC,
#ifdef TAS_NEEDS_EXCEPTION_EPILOGUE
			"defined",
#else  /* TAS_NEEDS_EXCEPTION_EPILOGUE */
			"not defined",
#endif /* TAS_NEEDS_EXCEPTION_EPILOGUE */
#ifdef TAS_NEEDS_RESTART
			"defined"
#else  /* TAS_NEEDS_RESTART */
			"not defined"
#endif /* TAS_NEEDS_RESTART */
		      );
	if (len >= MY_BUFSIZE) mybuf[MY_BUFSIZE] = '\0';

	if ( pos+count >= len ) {
		count = len-pos;
	}
	memcpy (buf, mybuf+pos, count);
	return count;
}


enum tas_state {not_initialized, now_initalizing, available}  ;
static volatile enum tas_state tas_dev_presence = not_initialized;

#define tas_dev_init init_module

#ifdef MODULE

void
cleanup_module (void)
{
	/* free code buffer */
	try_free_code_buffer();

	/* unregister /proc entry */
	(void) remove_proc_entry(TAS_DEVICE_NAME, NULL);

	/* unregister chrdev */
	unregister_chrdev(tas_major, TAS_DEVICE_NAME);

	tas_dev_presence=not_initialized;
}

#endif /* MODULE */


int __init tas_dev_init(void)
{

	int result;

	spin_lock(&lock);
	if (tas_dev_presence != not_initialized) {
		spin_unlock(&lock);
		return 0;
	}
	tas_dev_presence = now_initalizing;
	spin_unlock(&lock);

	result = register_chrdev(tas_major, TAS_DEVICE_NAME , &tas_fops);
	if (result < 0) {
		printk(KERN_WARNING 
		       TAS_DEVICE_NAME ": can't get major %d\n",tas_major);
		return result;
	}
	if (tas_major == 0) tas_major = result; /* dynamic */

	/* register /proc entry */
	if (!create_proc_info_entry(TAS_DEVICE_NAME, 0, NULL, &get_tas_info)) {
		printk(KERN_WARNING 
		       TAS_DEVICE_NAME ": can't get proc entry\n");
		unregister_chrdev(tas_major, TAS_DEVICE_NAME);
		return result;
	}

	(void) try_init_code_buffer();
	tas_dev_presence = available;

	return 0;
}

#ifndef MODULE
__initcall(tas_dev_init);
#endif


//========================================================================

/*
 * VMA Opreations
 */

static void tas_vma_open(struct vm_area_struct *vma)
{
    MOD_INC_USE_COUNT;
}

static void tas_vma_close(struct vm_area_struct *vma)
{
    MOD_DEC_USE_COUNT;
}

struct page *
tas_vma_nopage (struct vm_area_struct * area, 
			unsigned long address, int write_access)
{
	if ( address  != _TAS_START_MAGIC 
	    || area->vm_start  != _TAS_START_MAGIC
	    || area->vm_pgoff != 0 )
		return 0;

	get_page(tas_code_buffer);
	flush_page_to_ram(tas_code_buffer);
	return tas_code_buffer;
}


static struct vm_operations_struct tas_vm_ops = {
	open:tas_vma_open,
	close:tas_vma_close,
	nopage:tas_vma_nopage,
};

//========================================================================

/*
 * 	Device File Operations
 */


/*
 * Open and Close
 */

static int tas_open (struct inode *p_inode, struct file *p_file)
{
	
	int ret_code;
        if ( p_file->f_mode & FMODE_WRITE ) {
                return -EPERM;
        }
	
	ret_code =  try_init_code_buffer ();
	if (ret_code) {
		return ret_code;
	}

	/* 
	 * if you want store something for later processing, do it on
	 * p_file->private_data .
	 */
        MOD_INC_USE_COUNT;
        return 0;          /* success */
}

static int tas_release (struct inode *p_inode, struct file *p_file)
{
	MOD_DEC_USE_COUNT;
	return 0;
}


/*
 * Mmap
 */
static int tas_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long size;

	if (vma->vm_start != _TAS_START_MAGIC)
		return -ENXIO;

	if (vma->vm_pgoff != 0)
		return -ENXIO;

	size = vma->vm_end - vma->vm_start;
	if (size != PAGE_SIZE)
		return -EINVAL;

	vma->vm_ops = &tas_vm_ops;

	tas_vma_open(vma);

	return 0;
}


/*
 * Read
 */
static ssize_t tas_read(struct file *p_file, char * p_buff, size_t count, 
		   loff_t * p_pos)
{
	
	struct _tas_area_info info;
	int data;
	struct inode * p_inode;
	int info_size = sizeof(info);

	p_inode = p_file->f_dentry->d_inode;
	data = MAJOR(p_inode->i_rdev);

	info.magic = _TAS_INFO_MAGIC;
	info.pad1 = 0;
	info.map_addr = (void *)_TAS_START_MAGIC;
#if _MIPS_SZPTR==32
	info.pad2 = 0;
#endif

	if (*p_pos + count >= info_size){
		count = info_size - *p_pos;
	}
	if(copy_to_user(p_buff,((char *)&info)+*p_pos, count)) {
		return -EFAULT;
	}
	*p_pos += count;
	return count;
}

static
struct file_operations  tas_fops = {
	/* ssize_t (*read) (struct file *, char *, size_t, loff_t *); */
	read:tas_read,
	/* int (*open) (struct inode *, struct file *); */
	open:tas_open,
	/* int (*release) (struct inode *, struct file *);*/
	release:tas_release, 
	/* int (*mmap) (struct file *, struct vm_area_struct *); */
	mmap:tas_mmap,
};
