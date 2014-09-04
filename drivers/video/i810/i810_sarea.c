/*-*- linux-c -*-
 *  linux/drivers/video/i810_sarea.c -- Shared Area Allocation
 *
 *      Copyright (C) 2001 Antonino Daplas
 *      All Rights Reserved      
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */
#include <linux/types.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,4,14)
#include <linux/malloc.h>
#else
#include <linux/slab.h>
#endif

#include "i810_regs.h"
#include "i810_common.h"
#include "i810_sarea.h"


inline void i810fb_write_sarea_reg(void)
{
	i810_writel(HWS_PGA, i810_iface->i810_sarea_memory->physical);  
}

/**
 * i810fb_set/clear_gttmap - updates/clears the gtt usage map
 * @surface: pointer to agp_memory 
 */
inline void i810fb_set_gttmap(agp_memory *surface)
{
	int i;

	for (i = surface->pg_start; 
	     i < surface->pg_start + surface->page_count; 
	     i++) 
		set_bit(i, i810_iface->gtt_map); 
}

inline void i810fb_clear_gttmap(agp_memory *surface)
{
	int i;

	for (i = surface->pg_start; 
	     i < surface->pg_start + surface->page_count; 
	     i++)
		clear_bit(i, i810_iface->gtt_map);
}

int i810fb_bind_iface_mem(void) {
	struct list_head *list;
	agp_mem_struct *agp_list;

	if (!i810_iface->i810_sarea_memory->is_bound) {
		if (agp_bind_memory(i810_iface->i810_sarea_memory,
				    i810_iface->sarea_offset)) {
			printk("i810fb: can't rebind sarea memory\n");
			return -EBUSY;
		}
	}
	list_for_each(list, &i810_iface->agp_list_head) {
		agp_list = (agp_mem_struct *) list;
		if (!agp_list->surface->is_bound &&
		    agp_bind_memory(agp_list->surface, 
				    agp_list->surface->pg_start)) {
			printk("i810fb: can't rebind client memory\n");
			return -EBUSY;
		}
	}
	return 0;
}


inline u32 i810fb_get_sarea_start(void)
{
	return i810_iface->sarea_start_phys;
}

inline void i810_set_iface_lockup(void)
{
	i810_iface->lockup = 1;
}

void i810fb_unbind_iface_mem(void) {
	struct list_head *list;
	agp_mem_struct *agp_list;

	list_for_each(list, &i810_iface->agp_list_head) {
		agp_list = (agp_mem_struct *) list;
		if (!agp_list->surface->is_bound) 
			agp_unbind_memory(agp_list->surface);
	}
	if (i810_iface->i810_sarea_memory->is_bound) 
		agp_unbind_memory(i810_iface->i810_sarea_memory);
}

static int __devinit i810fb_alloc_resmem(void)
{
	if (!(i810_iface->gtt_map = vmalloc(GTT_SIZE >> 3))) 
		return 1;
	memset((void *) i810_iface->gtt_map, 0, GTT_SIZE >> 3);
	if (!(i810_iface->user_key_list = vmalloc(MAX_KEY >> 3))) 
		return 1;
	memset((void *) i810_iface->user_key_list, 0, 
	       MAX_KEY >> 3);
	INIT_LIST_HEAD(&i810_iface->agp_list_head);
	if (!(i810_iface->has_sarea_list = vmalloc(MAX_KEY >> 3))) 
		return 1;
	memset ((void *) i810_iface->has_sarea_list, 0,
		MAX_KEY >> 3);
	return 0;
}

static int __devinit i810fb_alloc_sharedmem(void)
{
	if (!(i810_iface->i810_sarea_memory = 
	      agp_allocate_memory(SAREA_SIZE >> 12, AGP_PHYSICAL_MEMORY))) 
		return 1;
	return agp_bind_memory(i810_iface->i810_sarea_memory,
			    i810_iface->sarea_offset);
}

void __devinit i810fb_fix_iface_pointers(u32 fb_size, u32 aper_size,
					 u32 fb_base_phys, u32 fb_base_virtual,
					 u32 cursor_phys, u32 cursor_virtual)
{
	i810_iface->sarea_start_phys = fb_base_phys + 
		(i810_iface->sarea_offset << 12);
	i810_iface->sarea_start_virt = fb_base_virtual +
		(i810_iface->sarea_offset << 12);
	
	i810_iface->sarea = (i810_sarea *) i810_iface->sarea_start_virt;
	memset((void *) i810_iface->sarea, 0, SAREA_SIZE);
	
	(u32) i810_iface->sarea->cur_surface_key = MAX_KEY;
	(u32) i810_iface->sarea->cur_user_key = MAX_KEY;
	(u32) i810_iface->cur_dma_buf_virt = 0;
	(u32) i810_iface->cur_dma_buf_phys = 0;
	i810_iface->fb_size = fb_size;
	i810_iface->aper_size = aper_size;
	i810_iface->fb_base_phys = fb_base_phys;
	i810_iface->fb_base_virt = fb_base_virtual;
	i810_iface->ovl_start_phys = cursor_phys + 1024;
	i810_iface->ovl_start_virtual = (u32 *) (cursor_virtual + 1024);
}

int __devinit i810fb_init_iface(u32 sarea_offset)
{
	if(!(i810_iface = kmalloc(sizeof(struct iface_data), GFP_KERNEL)))
		return -ENOMEM;
	memset(i810_iface, 0, sizeof(struct iface_data));
	i810_iface->sarea_offset = sarea_offset;
	if (i810fb_alloc_sharedmem()) return -ENOMEM;
	if (i810fb_alloc_resmem()) return -ENOMEM;
	return 0;
}

/**
 * i810fb_release_all - release all agpmemory on a per user key basis
 * @key: user key of the currently active process
 *
 * DESCRIPTION:
 * Walks through the linked list, and all agpmemory that matches the user key
 * will be unbound, deleted and removed from the list.
 */
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,4,9)
void i810fb_release_all(u32 key)
{
	struct list_head *list1, *list2;
	agp_mem_struct *agp_list;

	i810fb_sync();
	list_for_each_safe(list1, list2, &i810_iface->agp_list_head) {
		agp_list = (agp_mem_struct *) list1;
		if (agp_list->user_key == key) {
			i810fb_clear_gttmap(agp_list->surface);
			agp_unbind_memory(agp_list->surface);
			agp_free_memory(agp_list->surface);
			list_del(list1);
		}
	}
}

#else
void i810fb_release_all(u32 key)
{
	struct list_head *list ;
	agp_mem_struct *agp_list;

	wait_for_engine_idle();
	list_for_each(list, &i810_iface->agp_list_head) {
		agp_list = (agp_mem_struct *) list;
		if (agp_list->user_key == key) {
			i810fb_clear_gttmap(agp_list->surface);
			agp_unbind_memory(agp_list->surface);
			agp_free_memory(agp_list->surface);
			list_del(list);
		}
	}
}
#endif 

void i810fb_iface_cleanup(void)
{
	u32 i;

	if (i810_iface) {
		if (i810_iface->user_key_list) {
			for (i = 0; i < MAX_KEY; i++) {
				if (test_bit(i, i810_iface->user_key_list))
					i810fb_release_all(i);
			}
			vfree(i810_iface->user_key_list);
		}
		if (i810_iface->gtt_map)
			vfree(i810_iface->gtt_map);
		if (i810_iface->has_sarea_list)
			vfree(i810_iface->has_sarea_list);
		if (i810_iface->i810_sarea_memory)
			agp_free_memory(i810_iface->i810_sarea_memory);
		kfree(i810_iface);
	}
}

