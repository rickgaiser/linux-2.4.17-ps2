/*
 *  linux/drivers/misc/ucb1x00-core.c
 *
 *  Copyright (C) 2001 Russell King, All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 *
 *  The UCB1x00 core driver provides basic services for handling IO,
 *  the ADC, interrupts, and accessing registers.  It is designed
 *  such that everything goes through this layer, thereby providing
 *  a consistent locking methodology, as well as allowing the drivers
 *  to be used on other non-MCP-enabled hardware platforms.
 *
 *  Note that all locks are private to this file.  Nothing else may
 *  touch them.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/tqueue.h>
#include <linux/config.h>

#include <asm/irq.h>
#include <asm/mach-types.h>

#ifndef CONFIG_ARCH_PXA
#include <asm/arch/assabet.h>
#include <asm/arch/shannon.h>
#endif

#include <asm/hardware.h>

#include "ucb1x00.h"

/**
 *	ucb1x00_io_set_dir - set IO direction
 *	@ucb: UCB1x00 structure describing chip
 *	@in:  bitfield of IO pins to be set as inputs
 *	@out: bitfield of IO pins to be set as outputs
 *
 *	Set the IO direction of the ten general purpose IO pins on
 *	the UCB1x00 chip.  The @in bitfield has priority over the
 *	@out bitfield, in that if you specify a pin as both input
 *	and output, it will end up as an input.
 *
 *	ucb1x00_enable must have been called to enable the comms
 *	before using this function.
 *
 *	This function takes a spinlock, disabling interrupts.
 */
void ucb1x00_io_set_dir(struct ucb1x00 *ucb, unsigned int in, unsigned int out)
{
	unsigned long flags;

	spin_lock_irqsave(&ucb->io_lock, flags);
	ucb->io_dir |= out;
	ucb->io_dir &= ~in;

	ucb1x00_reg_write(ucb, UCB_IO_DIR, ucb->io_dir);
	spin_unlock_irqrestore(&ucb->io_lock, flags);
}

/**
 *	ucb1x00_io_write - set or clear IO outputs
 *	@ucb:   UCB1x00 structure describing chip
 *	@set:   bitfield of IO pins to set to logic '1'
 *	@clear: bitfield of IO pins to set to logic '0'
 *
 *	Set the IO output state of the specified IO pins.  The value
 *	is retained if the pins are subsequently configured as inputs.
 *	The @clear bitfield has priority over the @set bitfield -
 *	outputs will be cleared.
 *
 *	ucb1x00_enable must have been called to enable the comms
 *	before using this function.
 *
 *	This function takes a spinlock, disabling interrupts.
 */
void ucb1x00_io_write(struct ucb1x00 *ucb, unsigned int set, unsigned int clear)
{
	unsigned long flags;

	spin_lock_irqsave(&ucb->io_lock, flags);
	ucb->io_out |= set;
	ucb->io_out &= ~clear;

	ucb1x00_reg_write(ucb, UCB_IO_DATA, ucb->io_out);
	spin_unlock_irqrestore(&ucb->io_lock, flags);
}

/**
 *	ucb1x00_io_read - read the current state of the IO pins
 *	@ucb: UCB1x00 structure describing chip
 *
 *	Return a bitfield describing the logic state of the ten
 *	general purpose IO pins.
 *
 *	ucb1x00_enable must have been called to enable the comms
 *	before using this function.
 *
 *	This function does not take any semaphores or spinlocks.
 */
unsigned int ucb1x00_io_read(struct ucb1x00 *ucb)
{
	return ucb1x00_reg_read(ucb, UCB_IO_DATA);
}

/*
 * UCB1300 data sheet says we must:
 *  1. enable ADC	=> 5us (including reference startup time)
 *  2. select input	=> 51*tsibclk  => 4.3us
 *  3. start conversion	=> 102*tsibclk => 8.5us
 * (tsibclk = 1/11981000)
 * Period between SIB 128-bit frames = 10.7us
 */

/**
 *	ucb1x00_adc_enable - enable the ADC converter
 *	@ucb: UCB1x00 structure describing chip
 *
 *	Enable the ucb1x00 and ADC converter on the UCB1x00 for use.
 *	Any code wishing to use the ADC converter must call this
 *	function prior to using it.
 *
 *	This function takes the ADC semaphore to prevent two or more
 *	concurrent uses, and therefore may sleep.  As a result, it
 *	can only be called from process context, not interrupt
 *	context.
 *
 *	You should release the ADC as soon as possible using
 *	ucb1x00_adc_disable.
 */
void ucb1x00_adc_enable(struct ucb1x00 *ucb)
{
	down(&ucb->adc_sem);

	ucb->adc_cr |= UCB_ADC_ENA;

	ucb1x00_enable(ucb);
	ucb1x00_reg_write(ucb, UCB_ADC_CR, ucb->adc_cr);
}

/**
 *	ucb1x00_adc_read - read the specified ADC channel
 *	@ucb: UCB1x00 structure describing chip
 *	@adc_channel: ADC channel mask
 *	@sync: wait for syncronisation pulse.
 *
 *	Start an ADC conversion and wait for the result.  Note that
 *	synchronised ADC conversions (via the ADCSYNC pin) must wait
 *	until the trigger is asserted and the conversion is finished.
 *
 *	This function currently spins waiting for the conversion to
 *	complete (2 frames max without sync).
 *
 *	If called for a synchronised ADC conversion, it may sleep
 *	with the ADC semaphore held.
 *	
 *	See ucb1x00.h for definition of the UCB_ADC_DAT macro.  It
 *	addresses a bug in the ucb1200/1300 which, of course, Philips
 *	decided to finally fix in the ucb1400 ;-) -jws
 */
unsigned int ucb1x00_adc_read(struct ucb1x00 *ucb, int adc_channel, int sync)
{
	unsigned int val;

	if (sync)
		adc_channel |= UCB_ADC_SYNC_ENA;

	ucb1x00_reg_write(ucb, UCB_ADC_CR, ucb->adc_cr | adc_channel);
	ucb1x00_reg_write(ucb, UCB_ADC_CR, ucb->adc_cr | adc_channel | UCB_ADC_START);

	for (;;) {
		val = ucb1x00_reg_read(ucb, UCB_ADC_DATA);
		if (val & UCB_ADC_DAT_VAL)
			break;
		/* yield to other processes */
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(1);
	}

	return UCB_ADC_DAT(val);
}

/**
 *	ucb1x00_adc_disable - disable the ADC converter
 *	@ucb: UCB1x00 structure describing chip
 *
 *	Disable the ADC converter and release the ADC semaphore.
 */
void ucb1x00_adc_disable(struct ucb1x00 *ucb)
{
	ucb->adc_cr &= ~UCB_ADC_ENA;
	ucb1x00_reg_write(ucb, UCB_ADC_CR, ucb->adc_cr);
	ucb1x00_disable(ucb);

	up(&ucb->adc_sem);
}

/*
 * UCB1x00 Interrupt handling.
 *
 * The UCB1x00 can generate interrupts when the SIBCLK is stopped.
 * Since we need to read an internal register, we must re-enable
 * SIBCLK to talk to the chip.  We leave the clock running until
 * we have finished processing all interrupts from the chip.
 *
 * A restriction with interrupts exists when using the ucb1400, as
 * the codec read/write routines may sleep while waiting for codec
 * access completion and uses semaphores for access control to the
 * AC97 bus.  A complete codec read cycle could take  anywhere from
 * 60 to 100uSec so we *definitely* don't want to spin inside the
 * interrupt handler waiting for codec access.  So, we handle the
 * interrupt by scheduling a RT kernel thread to run in process
 * context instead of interrupt context.
 */

static int ucb1x00_thread(void *_ucb)
{
	struct task_struct *tsk = current;
	DECLARE_WAITQUEUE(wait, tsk);
	struct ucb1x00 *ucb = _ucb;
	struct ucb1x00_irq *irq;
	unsigned int isr, i;

	ucb->rtask = tsk;

	daemonize();
	reparent_to_init();
	tsk->tty = NULL;
	tsk->policy = SCHED_FIFO;
	tsk->rt_priority = 1;
	strcpy(tsk->comm, "kUCB1x00d");

	/* only want to receive SIGKILL */
	spin_lock_irq(&tsk->sigmask_lock);
	siginitsetinv(&tsk->blocked, sigmask(SIGKILL));
	recalc_sigpending(tsk);
	spin_unlock_irq(&tsk->sigmask_lock);

	add_wait_queue(&ucb->irq_wait, &wait);
	set_task_state(tsk, TASK_INTERRUPTIBLE);
	complete(&ucb->complete);

	for (;;) {
		if (signal_pending(tsk))
			break;
		enable_irq(ucb->irq);
		schedule();

		ucb1x00_enable(ucb);
		isr = ucb1x00_reg_read(ucb, UCB_IE_STATUS);
		ucb1x00_reg_write(ucb, UCB_IE_CLEAR, isr);
		ucb1x00_reg_write(ucb, UCB_IE_CLEAR, 0);

		for (i = 0, irq = ucb->irq_handler;
		     i < 16 && isr; 
		     i++, isr >>= 1, irq++)
			if (isr & 1 && irq->fn)
				irq->fn(i, irq->devid);
		ucb1x00_disable(ucb);

		set_task_state(tsk, TASK_INTERRUPTIBLE);
	}

	remove_wait_queue(&ucb->irq_wait, &wait);
	ucb->rtask = NULL;
	complete_and_exit(&ucb->complete, 0);
}

static void ucb1x00_irq(int irqnr, void *devid, struct pt_regs *regs)
{
	struct ucb1x00 *ucb = devid;
	disable_irq(irqnr);
	wake_up(&ucb->irq_wait);
}

/**
 *	ucb1x00_hook_irq - hook a UCB1x00 interrupt
 *	@ucb:   UCB1x00 structure describing chip
 *	@idx:   interrupt index
 *	@fn:    function to call when interrupt is triggered
 *	@devid: device id to pass to interrupt handler
 *
 *	Hook the specified interrupt.  You can only register one handler
 *	for each interrupt source.  The interrupt source is not enabled
 *	by this function; use ucb1x00_enable_irq instead.
 *
 *	Interrupt handlers will be called with other interrupts enabled.
 *
 *	Returns zero on success, or one of the following errors:
 *	 -EINVAL if the interrupt index is invalid
 *	 -EBUSY if the interrupt has already been hooked
 */
int ucb1x00_hook_irq(struct ucb1x00 *ucb, unsigned int idx, void (*fn)(int, void *), void *devid)
{
	struct ucb1x00_irq *irq;
	int ret = -EINVAL;

	if (idx < 16) {
		irq = ucb->irq_handler + idx;
		ret = -EBUSY;

		spin_lock_irq(&ucb->lock);
		if (irq->fn == NULL) {
			irq->devid = devid;
			irq->fn = fn;
			ret = 0;
		}
		spin_unlock_irq(&ucb->lock);
	}
	return ret;
}

/**
 *	ucb1x00_enable_irq - enable an UCB1x00 interrupt source
 *	@ucb: UCB1x00 structure describing chip
 *	@idx: interrupt index
 *	@edges: interrupt edges to enable
 *
 *	Enable the specified interrupt to trigger on %UCB_RISING,
 *	%UCB_FALLING or both edges.  The interrupt should have been
 *	hooked by ucb1x00_hook_irq.
 */
void ucb1x00_enable_irq(struct ucb1x00 *ucb, unsigned int idx, int edges)
{
	unsigned long flags;

	if (idx < 16) {
		spin_lock_irqsave(&ucb->lock, flags);

		ucb1x00_enable(ucb);

		/* This prevents spurious interrupts on the UCB1400 */
		ucb1x00_reg_write(ucb, UCB_IE_CLEAR, 1 << idx);
		ucb1x00_reg_write(ucb, UCB_IE_CLEAR, 0);

		if (edges & UCB_RISING) {
			ucb->irq_ris_enbl |= 1 << idx;
			ucb1x00_reg_write(ucb, UCB_IE_RIS, ucb->irq_ris_enbl);
		}
		if (edges & UCB_FALLING) {
			ucb->irq_fal_enbl |= 1 << idx;
			ucb1x00_reg_write(ucb, UCB_IE_FAL, ucb->irq_fal_enbl);
		}
		ucb1x00_disable(ucb);
		spin_unlock_irqrestore(&ucb->lock, flags);
	}
}

/**
 *	ucb1x00_disable_irq - disable an UCB1x00 interrupt source
 *	@ucb: UCB1x00 structure describing chip
 *	@edges: interrupt edges to disable
 *
 *	Disable the specified interrupt triggering on the specified
 *	(%UCB_RISING, %UCB_FALLING or both) edges.
 */
void ucb1x00_disable_irq(struct ucb1x00 *ucb, unsigned int idx, int edges)
{
	unsigned long flags;

	if (idx < 16) {
		spin_lock_irqsave(&ucb->lock, flags);

		ucb1x00_enable(ucb);
		if (edges & UCB_RISING) {
			ucb->irq_ris_enbl &= ~(1 << idx);
			ucb1x00_reg_write(ucb, UCB_IE_RIS, ucb->irq_ris_enbl);
		}
		if (edges & UCB_FALLING) {
			ucb->irq_fal_enbl &= ~(1 << idx);
			ucb1x00_reg_write(ucb, UCB_IE_FAL, ucb->irq_fal_enbl);
		}
		ucb1x00_disable(ucb);
		spin_unlock_irqrestore(&ucb->lock, flags);
	}
}

/**
 *	ucb1x00_free_irq - disable and free the specified UCB1x00 interrupt
 *	@ucb: UCB1x00 structure describing chip
 *	@idx: interrupt index
 *	@devid: device id.
 *
 *	Disable the interrupt source and remove the handler.  devid must
 *	match the devid passed when hooking the interrupt.
 *
 *	Returns zero on success, or one of the following errors:
 *	 -EINVAL if the interrupt index is invalid
 *	 -ENOENT if devid does not match
 */
int ucb1x00_free_irq(struct ucb1x00 *ucb, unsigned int idx, void *devid)
{
	struct ucb1x00_irq *irq;
	int ret;

	if (idx >= 16)
		goto bad;

	irq = ucb->irq_handler + idx;
	ret = -ENOENT;

	spin_lock_irq(&ucb->lock);
	if (irq->devid == devid) {
		ucb->irq_ris_enbl &= ~(1 << idx);
		ucb->irq_fal_enbl &= ~(1 << idx);

		ucb1x00_enable(ucb);
		ucb1x00_reg_write(ucb, UCB_IE_RIS, ucb->irq_ris_enbl);
		ucb1x00_reg_write(ucb, UCB_IE_FAL, ucb->irq_fal_enbl);
		ucb1x00_disable(ucb);

		irq->fn = NULL;
		irq->devid = NULL;
		ret = 0;
	}
	spin_unlock_irq(&ucb->lock);
	return ret;

bad:
	printk(KERN_ERR __FUNCTION__ ": freeing bad irq %d\n", idx);
	return -EINVAL;
}

/*
 * This configures the UCB1x00 layer depending on the machine type
 * we're running on.  The UCB1x00 drivers should not contain any
 * machine dependencies.
 */
static void ucb1x00_configure(struct ucb1x00 *ucb)
{
	unsigned int irq_gpio_pin = 0;
#ifdef CONFIG_ARCH_SA1100
	if (machine_is_adsbitsy())
		ucb->irq = IRQ_GPCIN4;

	if (machine_is_assabet()) {
		ucb->irq = IRQ_GPIO23;
		irq_gpio_pin = GPIO_GPIO23;
	}
#endif
#ifdef CONFIG_ARCH_LUBBOCK
	if (machine_is_lubbock()) {
		ucb->irq = LUBBOCK_UCB1400_IRQ;
	}
#endif
#ifdef CONFIG_ARCH_PXA_IDP
	if (machine_is_pxa_idp()) {
		ucb->irq = TOUCH_PANEL_IRQ;
		irq_gpio_pin = IRQ_TO_GPIO_2_80(TOUCH_PANEL_IRQ);
		GPDR(IRQ_TO_GPIO_2_80(TOUCH_PANEL_IRQ)) &= ~GPIO_bit(IRQ_TO_GPIO_2_80(TOUCH_PANEL_IRQ));
	}
#endif
#ifdef CONFIG_SA1100_CERF
	if (machine_is_cerf()) {
		ucb->irq = IRQ_GPIO_UCB1200_IRQ;
		irq_gpio_pin = GPIO_UCB1200_IRQ;
	}
#endif
#ifdef CONFIG_SA1100_FLEXANET
	if (machine_is_flexanet()) {
		ucb->irq = IRQ_GPIO_GUI;
		irq_gpio_pin = GPIO_GUI_IRQ;
	}
#endif
#ifdef CONFIG_SA1100_FREEBIRD
	if (machine_is_freebird()) {
		ucb->irq = IRQ_GPIO_FREEBIRD_UCB1300_IRQ;
		irq_gpio_pin = GPIO_FREEBIRD_UCB1300;
	}
#endif
#if defined(CONFIG_SA1100_GRAPHICSCLIENT) || defined(CONFIG_SA1100_GRAPICSMASTER)
	if (machine_is_graphicsclient() || machine_is_graphicsmaster()) {
		ucb->irq = ADS_EXT_IRQ(8);
	}
#endif
#ifdef CONFIG_SA1100_LART
	if (machine_is_lart()) {
		ucb->irq = LART_IRQ_UCB1200;
		irq_gpio_pin = LART_GPIO_UCB1200;
	}
#endif
#ifndef CONFIG_ARCH_PXA
	if (machine_is_omnimeter())
		ucb->irq = IRQ_GPIO23;
#endif
#ifdef CONFIG_SA1100_PFS168
	if (machine_is_pfs168()) {
		ucb->irq = IRQ_GPIO_UCB1300_IRQ;
		irq_gpio_pin = GPIO_UCB1300_IRQ;
	}
#endif
#ifdef CONFIG_SA1100_SIMPAD
	if (machine_is_simpad()) {
		ucb->irq = IRQ_GPIO_UCB1300_IRQ;
		irq_gpio_pin = GPIO_UCB1300_IRQ;
	}
#endif
#ifndef CONFIG_ARCH_PXA
	if (machine_is_shannon()) {
		ucb->irq = SHANNON_IRQ_GPIO_IRQ_CODEC;
		irq_gpio_pin = SHANNON_GPIO_IRQ_CODEC;
	}
#endif
#ifdef CONFIG_SA1100_YOPY
	if (machine_is_yopy()) {
		ucb->irq = IRQ_GPIO_UCB1200_IRQ;
		irq_gpio_pin = GPIO_UCB1200_IRQ;
	}
#endif

	if (irq_gpio_pin) {
#ifdef CONFIG_ARCH_PXA_IDP
		set_GPIO_IRQ_edge(irq_gpio_pin, TOUCH_PANEL_IRQ_EDGE);
#else	
		set_GPIO_IRQ_edge(irq_gpio_pin, GPIO_RISING_EDGE);
#endif
	}
}

struct ucb1x00 *my_ucb;

/**
 *	ucb1x00_get - get the UCB1x00 structure describing a chip
 *	@ucb: UCB1x00 structure describing chip
 *
 *	Return the UCB1x00 structure describing a chip.
 *
 *	FIXME: Currently very noddy indeed, which currently doesn't
 *	matter since we only support one chip.
 *	(and the ucb1400 is close enough to the 12/1300... -jws)
 */
struct ucb1x00 *ucb1x00_get(void)
{
	return my_ucb;
}

extern struct mcp mcp_sa1100;

static int __init ucb1x00_init(void)
{
	struct mcp *mcp;
	unsigned int id;
	int ret = -ENODEV;

	mcp = mcp_get();
	if (!mcp)
		goto no_mcp;

	mcp_enable(mcp);
	id = mcp_reg_read(mcp, UCB_ID);

	if (id != UCB_ID_1200 && id != UCB_ID_1300 && id != UCB_ID_1400) {
		printk(KERN_WARNING "UCB1x00 ID not found: %04x\n", id);
		goto out;
	}

	/* distinguish between UCB1400 revs 1B and 2A */
	if (id == UCB_ID_1400 && mcp_reg_read(mcp, 0x00) == 0x002a)
		id = UCB_ID_1400_BUGGY;

	my_ucb = kmalloc(sizeof(struct ucb1x00), GFP_KERNEL);
	ret = -ENOMEM;
	if (!my_ucb)
		goto out;
	memset(my_ucb, 0, sizeof(struct ucb1x00));

#ifdef CONFIG_ARCH_SA1100  /* why do I have to keep doing this crap??? */
	if (machine_is_shannon()) {
		/* reset the codec */
		GPDR |= SHANNON_GPIO_CODEC_RESET;
		GPCR = SHANNON_GPIO_CODEC_RESET;
		GPSR = SHANNON_GPIO_CODEC_RESET;
	}
#endif

	spin_lock_init(&my_ucb->lock);
	spin_lock_init(&my_ucb->io_lock);
	sema_init(&my_ucb->adc_sem, 1);

	my_ucb->id  = id;
	my_ucb->mcp = mcp;

	ucb1x00_reg_write(my_ucb, UCB_IE_RIS, 0);
	ucb1x00_reg_write(my_ucb, UCB_IE_FAL, 0);
	ucb1x00_reg_write(my_ucb, UCB_IE_CLEAR, 0xffff);
	ucb1x00_reg_write(my_ucb, UCB_IE_CLEAR, 0);
	ucb1x00_configure(my_ucb);

	init_waitqueue_head(&my_ucb->irq_wait);
	ret = request_irq(my_ucb->irq, ucb1x00_irq, 0, "UCB1x00", my_ucb);
	if (ret) {
		printk("ucb1x00: unable to grab irq%d: %d\n", my_ucb->irq, ret);
		goto irq_err;
	}

	init_completion(&my_ucb->complete);
	ret = kernel_thread(ucb1x00_thread, my_ucb, CLONE_FS | CLONE_FILES);
	if (ret >= 0) {
		wait_for_completion(&my_ucb->complete);
		ret = 0;
		goto out;
	}

	free_irq(my_ucb->irq, my_ucb);
irq_err:
	kfree(my_ucb);
	my_ucb = NULL;
out:
	mcp_disable(mcp);
no_mcp:
	return ret;
}

static void __exit ucb1x00_exit(void)
{
	send_sig(SIGKILL, my_ucb->rtask, 1);
	wait_for_completion(&my_ucb->complete);
	free_irq(my_ucb->irq, my_ucb);
	kfree(my_ucb);
}

module_init(ucb1x00_init);
module_exit(ucb1x00_exit);

EXPORT_SYMBOL(ucb1x00_get);

EXPORT_SYMBOL(ucb1x00_io_set_dir);
EXPORT_SYMBOL(ucb1x00_io_write);
EXPORT_SYMBOL(ucb1x00_io_read);

EXPORT_SYMBOL(ucb1x00_adc_enable);
EXPORT_SYMBOL(ucb1x00_adc_read);
EXPORT_SYMBOL(ucb1x00_adc_disable);

EXPORT_SYMBOL(ucb1x00_hook_irq);
EXPORT_SYMBOL(ucb1x00_free_irq);
EXPORT_SYMBOL(ucb1x00_enable_irq);
EXPORT_SYMBOL(ucb1x00_disable_irq);

MODULE_AUTHOR("Russell King <rmk@arm.linux.org.uk>");
MODULE_DESCRIPTION("UCB1x00 core driver");
MODULE_LICENSE("GPL");
