/*-*- linux-c -*-
 *  linux/drivers/video/i810_iface.c -- Hardware Interface
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
#include "i810_iface.h"

/*
 * Resource Management
 */

/**
 * i810fb_find_free_block - finds a block of unbound gart memory
 * @pgsize: size in pages (4096)
 *
 * DESCRIPTION:
 * This function finds a free block of gart memory as determined
 * by gtt_map
 *
 * RETURNS:
 * start page offset of block
 */
static int i810fb_find_free_block(u32 pgsize)
{
	u32 offset, cur_size = 0;

	offset = i810_iface->sarea_offset;
	while (cur_size < pgsize && offset) {
		offset--;
		if (!test_bit(offset, i810_iface->gtt_map)) 
			++cur_size;
		else if (cur_size < pgsize) 
			cur_size = 0;
	} 
	return (cur_size < pgsize) ? -1 : (int) offset;
}

/**
 * i810fb_allocate_agpmemory - allocates and binds agp memory
 * @agp_mem: pointer to agp_mem_user
 *
 * DESCRIPTION:
 * Allocates a requested agp memory type and size, then writes the surface
 * key and page offset to @agp_mem, if successful.  
 */
int i810fb_allocate_agpmemory(agp_mem_user *agp_mem)
{
	agp_mem_struct *new;

	if (!test_bit(agp_mem->user_key, i810_iface->user_key_list))
		return -EACCES;

	switch (agp_mem->type) {
	case AGP_DMA:
		if (agp_mem->pgsize > MAX_DMA_SIZE >> 12)
			return -EINVAL;
		if (!agp_mem->pgsize)
			agp_mem->pgsize = 4;
		break;
	case AGP_SURFACE:
		if (!agp_mem->pgsize)
			return -EINVAL;
		break;
	case AGP_SAREA:
		agp_mem->pgsize = SAREA_SIZE >> 12;
		agp_mem->surface_key = i810_iface->i810_sarea_memory->key;
		agp_mem->offset = ((i810_iface->fb_size + MMIO_SIZE + i810_iface->aper_size) >> 12) +
			agp_mem->user_key;
		return 0;
	default:
		return -EINVAL;
	}
	if (NULL == (new = vmalloc(sizeof(agp_mem_struct))))
		return -ENOMEM;
	memset((void *) new, 0, sizeof(new));
	agp_mem->offset = i810fb_find_free_block(agp_mem->pgsize);
	if (agp_mem->offset == -1)
		return -ENOMEM;
	new->surface = agp_allocate_memory(agp_mem->pgsize, AGP_NORMAL_MEMORY);
	if (new->surface == NULL) {
		vfree(new);
		return -ENOMEM;
	}
	if (agp_bind_memory(new->surface, agp_mem->offset)) {
		agp_free_memory(new->surface);
		vfree(new);
		return -EBUSY;
	}
	i810fb_set_gttmap(new->surface);
	new->surface_type = agp_mem->type;
	new->user_key = agp_mem->user_key;
	agp_mem->surface_key = new->surface->key;
	list_add(&new->agp_list, &i810_iface->agp_list_head);
	return 0;
}

/**
 * i810fb_free_agpmemory - allocates and binds agp memory
 * @agp_mem: pointer to agp_mem_user
 *
 * DESCRIPTION:
 * Free a previously requested agp memory. 
 */
int i810fb_free_agpmemory(agp_mem_user *agp_mem)
{
	struct list_head *list;
	agp_mem_struct *agp_list;

	list_for_each(list, &i810_iface->agp_list_head) {
		agp_list = (agp_mem_struct *) list;
		if (agp_list->surface->key == agp_mem->surface_key && 
		    agp_list->user_key == agp_mem->user_key &&
		    agp_list->surface->pg_start == agp_mem->offset &&
		    agp_list->surface->page_count == agp_mem->pgsize &&
		    agp_list->surface_type == agp_mem->type) {
			i810fb_sync();
			i810fb_clear_gttmap(agp_list->surface);
			agp_unbind_memory(agp_list->surface);
			agp_free_memory(agp_list->surface);
			list_del(list);
			return 0;
		}
	}
	return -EINVAL;
}

/**
 * i810fb_acquire_fb - acquires the framebuffer
 *
 * DESCRIPTION:
 * Acquires the framebuffer device.  If successful, returns a 
 * user key which should be passed to the fb  driver each time 
 * a service is requested.
 */
int i810fb_acquire_fb(void)
{
	int key;

	key = find_first_zero_bit(i810_iface->user_key_list, MAX_KEY);
	if (key >= MAX_KEY)
		return -1;
	if (i810fb_bind_all())	return -1;
	set_bit(key, i810_iface->user_key_list);
	return key;
}


/**
 * i810fb_check_mmap - check if area to be mmaped is valid
 * @offset: offset to start of aperture space
 *
 * DESCRIPTION:
 * Checks if @offset matches any of the agp memory in the list.
 */
int i810fb_check_agp_mmap(u32 offset, uid_t uid)
{
	struct list_head *list;
	agp_mem_struct *agp_list;
	
	list_for_each(list, &i810_iface->agp_list_head) {
		agp_list = (agp_mem_struct *) list;
		if (offset >= agp_list->surface->pg_start  && 
		    offset < agp_list->surface->pg_start + 
		    agp_list->surface->page_count) {
			if (!uid)
				agp_list->trusted = 1;
			else
				agp_list->trusted = 0;
			return ((agp_list->surface->pg_start + 
				 agp_list->surface->page_count) - 
				offset) << 12;
		}
	}
	return 0;

}

/**
 * i810fb_check_sarea - check if shared area can be mapped 
 * @offset: offset to map (equivalent to user key)
 * @uid: uid of requestor
 * 
 * DESCRIPTION:
 * This function checks of the sarea can be mapped to user space.
 */
int i810fb_check_sarea(u32 offset, uid_t uid)
{
	if (uid) {
		printk("sarea: go away, you don't have permission\n");
		return 0;
	}
	if (test_bit(offset, i810_iface->user_key_list)) {
		set_bit(offset, i810_iface->has_sarea_list);
		return SAREA_SIZE;
	}
	printk("sarea: invalid user key\n");
	return 0;
}

/**
 * i810fb_release_fb - release the framebuffer device
 * @command: pointer to i810_command
 *
 * DESCRIPTION:
 * Release the framebuffer device.  All allocated resources 
 * will be released, and the user key will be removed from the list.  
 */
static int i810fb_release_fb(i810_command *command)
{
	i810fb_release_all(command->user_key);
	clear_bit(command->user_key, i810_iface->user_key_list);
	clear_bit(command->user_key, i810_iface->has_sarea_list);
	(u32) i810_iface->cur_dma_buf_virt = 0;
	(u32) i810_iface->cur_dma_buf_phys = 0;
	(u32) i810_iface->sarea->cur_surface_key = MAX_KEY;
	(u32) i810_iface->sarea->cur_user_key = MAX_KEY;
	(u32) i810_iface->sarea->is_valid = 0;
	return 0;
}
			
/**
 * i810fb_update_dma - updates the current user DMA buffer pointer
 * @command: pointer to i810_command structure
 *
 */
static int i810fb_update_dma(i810_command *command)
{
	struct list_head *list;
	agp_mem_struct *agp_list;

	list_for_each(list, &i810_iface->agp_list_head) {
		agp_list = (agp_mem_struct *) list;
		if (agp_list->surface->key == command->surface_key && 
		    agp_list->user_key == command->user_key &&
		    agp_list->surface_type == AGP_DMA) {
			i810_iface->cur_dma_buf_virt = 
				(u32 *) (i810_iface->fb_base_virt + 
					 (agp_list->surface->pg_start << 12));
			i810_iface->cur_dma_buf_phys = 
				(u32 *) (i810_iface->fb_base_phys + 
					 (agp_list->surface->pg_start << 12));
			i810_iface->sarea->cur_surface_key = 
				command->surface_key;
			i810_iface->sarea->cur_user_key = command->user_key;
			i810_iface->sarea->is_valid = 1;
			i810_iface->trusted = agp_list->trusted;
			i810_iface->cur_dma_size = 
				agp_list->surface->page_count << 12;
			return 0;
		}
	}
	return -EINVAL;
}

/**
 * i810fb_parse_parser - verifies parser type instructions (opcode 00)
 * @pointer:  the offset to the current user DMA buffer
 * @dsize: the number of dwords from the offset to the end of the 
 * instruction packets
 *
 * DESCRIPTION:
 * Process parser-type instructions.  The only verification done is 
 * the size of the instruction on a per opcode basis.
 */

static int i810fb_parse_parser(u32 *pointer, u32 dsize)
{
	u32 cur_header;
	int i;
	
	cur_header = *pointer;
	if ((cur_header & (0x3F << 23)) < (0x09 << 23)) 
		i = 1;
	else
		i = (cur_header & 0x3F) + 2; 
	return (i > dsize || i > i810_iface->cur_dma_size >> 2) ?
		-1 : i;
}

/**
 * i810fb_parse_blitter - verifies blitter type instructions (opcode 02)
 * @pointer:  the offset to the current user DMA buffer
 * @dsize: the number of dwords from the offset to the end of 
 * the instruction packets
 *
 * DESCRIPTION:
 * Process blit-type instructions.  The only verification done is
 * checking the size of the instruction in dwords.
 */
static int i810fb_parse_blitter(u32 *pointer, u32 dsize)
{
	u32 cur_header;
	int i;

	cur_header = *pointer;
	i = (cur_header & 0x1F) + 2;
	return (i > dsize || i > i810_iface->cur_dma_size >> 2) ?
		-1 : i;
}

/**
 * i810fb_parse_render - verifies render type instructions (opcode 03)
 * @pointer:  the offset to the current user DMA buffer
 * @dsize: the number of dwords from the offset to the end of 
 * the instruction packets
 *
 * DESCRIPTION:
 * Process render-type instructions. It verifies the size of the packets based
 * on the opcode. All invalid opcodes will result in an error.
 */
static int i810fb_parse_render(u32 *pointer, u32 dsize)
{
	u32 cur_header, opcode;
	int i;

	cur_header = *pointer;
	opcode = cur_header & (0x1F << 24);
	
	switch(opcode) {
	case 0 ... (0x18 << 24):
	case (0x1C << 24):
		i = 1;
		break;
	case (0x1D << 24) ... (0x1E << 24):
		i = (cur_header & 0xFF) + 2;
		break;
	case (0x1F << 24):
		i = (cur_header & 0x3FF) + 2;
		break;
	default:
		return -1; 
	}
	return (i > dsize || i > i810_iface->cur_dma_size >> 2) ?
	        -1 : i;
}

/**
 * process_buffer_with_verify - process command buffer contents
 * @v_pointer: virtual pointer to start of instruction;
 * @p: physical pointer to start of instruction
 * @dsize: length of instruction sequence in dwords
 * @command: pointer to i810_command
 *
 * DESCRIPTION:
 * Processes command buffer instructions prior to execution.  This 
 * includes verification of each instruction header for validity.
 * This is reserved for clients which are not trusted.
 */
static inline u32 process_buffer_with_verify(u32 v_pointer, u32 p, u32 dsize,
					    i810_command *command)
{
	u32  dcount = 0, i = 0, opcode;
	
	if (dsize & 1) {
		*((u32 *) (v_pointer + (dsize << 2))) = 0;
		dsize++;
	}
	do {
		opcode =  *((u32 *) v_pointer) & (0x7 << 29);
		switch (opcode) {
		case PARSER:
			i = i810fb_parse_parser((u32 *) v_pointer, 
						dsize);
			break;
		case BLIT:
			i = i810fb_parse_blitter((u32 *) v_pointer, 
						 dsize);
			break;
		case RENDER:
			i = i810fb_parse_render((u32 *) v_pointer, 
						dsize);
			break;
		default:
			i = -1;
		}
		if (i == -1) 
			break;
		v_pointer += i << 2;
		dcount += i;
		dsize -= i;
	} while (dsize);
	emit_instruction(dcount, p, 1); 
	i810fb_sync();
	return dsize;
}


/**
 * process_buffer_no_verify - process command buffer contents
 * @v_pointer: virtual pointer to start of instruction;
 * @p: physical pointer to start of instruction
 * @dsize: length of instruction sequence in dwords
 * @command: pointer to i810_command
 *
 * DESCRIPTION:
 * processes command buffer instructions prior to execution.  If
 * client has shared area, will update current head and tail.
 * This is reserved for trusted clients.
 */
static inline u32 process_buffer_no_verify(u32 v_pointer, u32 p, u32 dsize,
					    i810_command *command)
{
	u32 tail_pointer, tail;
		
	if (!test_bit(command->user_key, i810_iface->has_sarea_list)) {
		if (dsize & 1) {
			*((u32 *) (v_pointer + (dsize << 2))) = 0;
			dsize++;
		}
		emit_instruction(dsize, p, 0);
		i810fb_sync();
		return 0;
	}

	if (!(dsize & 1)) {
		*((u32 *) (v_pointer + (dsize << 2))) = 0;
		dsize++;
	}
	dsize += 3;
	
	tail = (command->dma_cmd_start + dsize) << 2;
	i810_iface->sarea->tail = tail;
	tail_pointer = v_pointer + ((dsize - 3) << 2);
	*((u32 *) (tail_pointer))      = PARSER | STORE_DWORD_IDX | 1;
	*((u32 *) (tail_pointer + 4))  = 7 << 2;
	*((u32 *) (tail_pointer + 8))  = tail;
	emit_instruction(dsize, p, 0);
	return 0;
}

static inline u32 process_overlay(u32 v_pointer, u32 dsize,
				  i810_command *command)
{
	u32 tail;
	int i, sarea = 0;
	
	if (dsize != 30)
		return -EINVAL;
	if (test_bit(command->user_key, i810_iface->has_sarea_list)) 
		sarea = 1;
	
	if (sarea) {
		tail = (command->dma_cmd_start + dsize) << 2;
		i810_iface->sarea->tail = tail;
	}

	i810fb_sync();
	for (i = 0; i < 30; i++) 
		i810_iface->ovl_start_virtual[i] = ((u32 *) v_pointer)[i];
	i810_writel(OVOADDR, i810_iface->ovl_start_phys | (1 << 31));

	if (sarea)
		i810_iface->sarea->head = i810_iface->sarea->tail;
	return 0;
}	

/**
 * i810fb_emit_dma - processes DMA instructions from client
 * @command: pointer to i810_command
 *
 * DESCRIPTION:
 * Clients cannot directly use the ringbuffer.  To instruct the hardware 
 * engine, the client writes all instruction packets to the current user 
 * DMA buffer, and tells the fb driver to process those instructions.  
 * The fb driver on the other hand, will verify if the packets are valid 
 * _if_ the instructions come from a nontrusted source (not root).  
 * Once verification is finished, the instruction sequence will be processed 
 * via batch buffers. If an invalid instruction is encountered, the sequence will
 * be truncated from that point and the function will exit with an 
 * error.  Instruction sequences can be chained, resulting in faster 
 * performance.  
 *
 * If the source is trusted, the verfication stage is skipped, resulting 
 * in greater performance at the expense of increasing the chances of 
 * locking the machine.  If the client is using shared memory, the start (head)
 * and end (tail) of the currently processed instruction sequence will be 
 * written to the shared area.
 */
static int i810fb_emit_dma(i810_command *command)
{

	u32 cur_pointer, phys_pointer, dsize, ret;
	
	if (i810_iface->lockup) return -EINVAL;
	if (i810_iface->sarea->cur_surface_key != command->surface_key) { 
		if (i810fb_update_dma(command))
			return -EACCES;
	}
	else if (i810_iface->sarea->cur_user_key != command->user_key)
		return -EINVAL;

	dsize = command->dma_cmd_dsize;
	if (dsize + command->dma_cmd_start > 
	    (i810_iface->cur_dma_size >> 2) - 3 || 
	    !dsize)
		return -EINVAL;
	phys_pointer = (u32) i810_iface->cur_dma_buf_phys + 
		(command->dma_cmd_start << 2);
	cur_pointer = (u32) i810_iface->cur_dma_buf_virt + 
		(command->dma_cmd_start << 2);

	switch(command->command) {
	case EMIT_DMA:
		if (!i810_iface->trusted) 
			ret = process_buffer_with_verify(cur_pointer, phys_pointer, dsize,
							 command);
		else
			ret = process_buffer_no_verify(cur_pointer, phys_pointer, dsize,
						       command);
		break;
	case EMIT_OVERLAY:
		ret = process_overlay(cur_pointer, dsize, command);
		break;
	default:
		ret = 1;
	}
	return (ret) ? -EINVAL : 0;
}


int i810fb_process_command(i810_command *command)
{
	switch (command->command) {
	case EMIT_DMA:
	case EMIT_OVERLAY:
		return i810fb_emit_dma(command);
	case RELEASE_FB:
		return i810fb_release_fb(command);
	default:
		return -EINVAL;
	}
}

