/*
 *	$Id: maple.c,v 1.1 2001/12/12 23:44:45 jsiegel Exp $
 *	Maple Bus device driver
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/malloc.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/smp_lock.h>
#include <linux/completion.h>

#include <asm/page.h>
#include <asm/io.h>

#include <linux/maple.h>

#define DPRINTK(format, args...) printk(KERN_DEBUG format,##args)

#define MAPLE_PORTS 4
#define MAPLE_SCANHZ (HZ/100)
#define MAPLE_PNP_INTERVAL HZ

#define MAPLE_MAXPACKETS 8
#define MAPLE_DMA_ORDER	14	/* 16K bytes */
#define MAPLE_DMA_SIZE	(1<<MAPLE_DMA_ORDER)
#define MAPLE_DMA_PAGES	((MAPLE_DMA_ORDER > PAGE_SHIFT) ? MAPLE_DMA_ORDER-PAGE_SHIFT : 0)

static LIST_HEAD(maple_dev_list);
static LIST_HEAD(maple_driver_list);
static LIST_HEAD(maple_waitq);
static LIST_HEAD(maple_sentq);
static DECLARE_WAIT_QUEUE_HEAD(kmapled_wait);
static DECLARE_COMPLETION(kmapled_exited);
static int kmapled_pid = 0;

struct timer_list maple_timer;

unsigned long *maple_sendbuf, *maple_sendptr, *maple_lastptr;
unsigned long maple_pnp_time;

static struct maple_driver maple_dummy_driver = {
	function:	0,
	name:		"Dummy",
};

static void maple_dump_devinfo(struct maple_devinfo *devinfo)
{
	DPRINTK("  function:     0x%08x\n", be32_to_cpu(devinfo->function));
	DPRINTK("   data[0]:     0x%08x\n", be32_to_cpu(devinfo->function_data[0]));
	DPRINTK("   data[1]:     0x%08x\n", be32_to_cpu(devinfo->function_data[1]));
	DPRINTK("   data[2]:     0x%08x\n", be32_to_cpu(devinfo->function_data[2]));
	DPRINTK("  area code:    %d\n", devinfo->area_code);
	DPRINTK("  direction:    %d\n", devinfo->connector_direction);
	DPRINTK("  name:         \"%-30.30s\"\n", devinfo->product_name);
	DPRINTK("  license:      \"%-60.60s\"\n", devinfo->product_license);
	DPRINTK("  stanby power: %d\n", le16_to_cpu(devinfo->standby_power));
	DPRINTK("  max power:    %d\n",  le16_to_cpu(devinfo->max_power));
}


void maple_register_driver(struct maple_driver *driver)
{
	struct list_head *lh = (void *)driver;
	list_add(lh, &maple_driver_list);

	MOD_INC_USE_COUNT;

	printk(KERN_INFO "maple: registered driver: %s (function 0x%lx)\n",
	       driver->name, driver->function);
}


void maple_unregister_driver(struct maple_driver *driver)
{
	struct list_head *lh = (void *)driver;
	list_del(lh);

	MOD_DEC_USE_COUNT;

	printk(KERN_INFO "maple: unregistered driver: %s (function 0x%lx)\n",
	       driver->name, driver->function);
}


static struct mapleq *maple_allocq(struct maple_device *dev)
{
	unsigned long buf;
	struct mapleq *mq;

	mq = kmalloc(sizeof(*mq), GFP_KERNEL);
	if (!mq)
		return NULL;

	mq->dev = dev;
	buf = (unsigned long)mq->buf;
	buf = (buf + 31) & ~31;
	mq->recvbuf = (void *)P2SEGADDR(buf);

	return mq;
}


static void maple_freeq(struct mapleq *mq)
{
	if (!mq)
		return;

	kfree(mq);
}


static struct maple_device *maple_alloc_dev(int port, int unit)
{
	struct maple_device *dev;

	dev = kmalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return NULL;

	memset(dev, 0, sizeof(*dev));

	dev->port = port;
	dev->unit = unit;
	dev->mq = maple_allocq(dev);

	if (!dev->mq) {
		kfree(dev);
		return NULL;
	}

	return dev;
}


static void maple_free_dev(struct maple_device *dev)
{
	if (!dev)
		return;

	if (dev->mq)
		maple_freeq(dev->mq);

	kfree(dev);
}


static void maple_register_dev(struct maple_device *dev)
{
	struct list_head *lh = (void *)dev;
	list_add_tail(lh, &maple_dev_list);
}


static void maple_unregister_dev(struct maple_device *dev)
{
	struct list_head *lh = (void *)dev;
	list_del(lh);
}


void maple_getcond_callback(struct maple_device *dev,
			    void (*callback)(struct mapleq *mq),
			    unsigned long interval, unsigned long function)
{
	dev->callback = callback;
	dev->interval = interval;
	dev->function = cpu_to_be32(function);
	dev->when = 0;
}


int maple_add_packet(struct mapleq *mq)
{
	unsigned long flags;

	save_and_cli(flags);
	list_add((struct list_head *)mq, &maple_waitq);
	restore_flags(flags);

	return 0;
}


int maple_del_packet(struct mapleq *mq)
{
	struct list_head *lh;
	unsigned long flags;
	
	save_and_cli(flags);
	
	list_for_each(lh, &maple_sentq) {
		if (mq == (struct mapleq *)lh) {
			restore_flags(flags);
			return -1;
		}
	}

	list_for_each(lh, &maple_waitq) {
		if (mq == (struct mapleq *)lh)
			list_del(lh);
	}

	restore_flags(flags);

	return 0;
}


static int maple_dma_done(void)
{
	return (ctrl_inl(MAPLE_STATE) & 1) == 0;
}


static void maple_attach_driver(struct maple_device *dev)
{
	struct list_head *lh;
	struct maple_driver *driver;
	unsigned long function;
	char *p, *recvbuf = dev->mq->recvbuf;

	memcpy(&dev->devinfo, recvbuf+4, sizeof(dev->devinfo));

	memcpy(dev->product_name, dev->devinfo.product_name, 30);
	memcpy(dev->product_license, dev->devinfo.product_license, 60);
	dev->product_name[30] = 0;
	dev->product_license[60] = 0;

	for (p=dev->product_name+29; dev->product_name<=p; p--)
		if (*p==' ') *p = 0; else break;
	
	for (p=dev->product_license+59; dev->product_license<=p; p--)
		if (*p==' ') *p = 0; else break;

	function = be32_to_cpu(dev->devinfo.function);

	printk(KERN_INFO "maple(%d,%d): Connected(function 0x%lx)\n",
	       dev->port, dev->unit, function);

	list_for_each(lh, &maple_driver_list) {
		driver = (struct maple_driver *)lh;
		if (function & driver->function) {
			if (!driver->connect(dev)) {
				dev->driver = driver;
				break;
			}
		}
	}

	if (dev->driver==NULL) {
		printk(KERN_INFO "maple(%d,%d): No proper driver.\n",
		       dev->port, dev->unit);
		maple_dump_devinfo(&dev->devinfo);
		dev->driver = &maple_dummy_driver;
	}

}


static void maple_detach_driver(struct maple_device *dev)
{
	printk(KERN_INFO "maple(%d,%d): Disconnected\n",
	       dev->port, dev->unit);

	dev->driver->disconnect(dev);
	dev->driver = NULL;
}


static void maple_dma_irq(void)
{
	struct mapleq *mq;
	struct maple_device *dev;
	struct list_head *lh, tmph;
	char *recvbuf;
	int code, need_wakeup = 0;

	if (list_empty(&maple_sentq) || !maple_dma_done()) return;

	list_for_each(lh, &maple_sentq) {
		mq = (struct mapleq *)lh;
		dev = mq->dev;
		tmph = *lh;
		lh = &tmph;

		recvbuf = mq->recvbuf;
		code = recvbuf[0];

		switch (code) {
		case MAPLE_RESPONSE_NONE:
		case MAPLE_RESPONSE_DEVINFO:
			maple_getcond_callback(dev, NULL, 0, 0);
			dev->event = code;
			need_wakeup = 1;
			break;

		case MAPLE_RESPONSE_DATATRF:
			if (dev->callback)
				dev->callback(mq);
			break;

		default:
			break;
		}

	}
	
	INIT_LIST_HEAD(&maple_sentq);

	if (need_wakeup)
		wake_up(&kmapled_wait);
}


static void maple_build_block(struct mapleq *mq)
{
	int port, unit, from, to, len;
	unsigned long *lsendbuf = mq->sendbuf;

	port = mq->dev->port & 3;
	unit = mq->dev->unit;
	len = mq->length;

	from = port<<6;
	to = (port<<6) | (unit>0 ? (1<<(unit-1))&0x1f : 0x20);

	*maple_lastptr &= 0x7fffffff;
	maple_lastptr = maple_sendptr;

        *maple_sendptr++ = (port<<16) | len | 0x80000000;
	*maple_sendptr++ = PHYSADDR(mq->recvbuf);
	*maple_sendptr++ = mq->command | (to<<8) | (from<<16) | (len<<24);

	while (len-->0) {
		*maple_sendptr++ = *lsendbuf++;
	}
}


void maple_send(void)
{
	int i;
	int maple_packets;
	struct mapleq *mq;
	struct list_head *lh, tmph;

	if (!list_empty(&maple_sentq))
		return;

	if (list_empty(&maple_waitq) || !maple_dma_done())
		return;

	maple_packets = 0;
	maple_sendptr = maple_lastptr = maple_sendbuf;

	list_for_each(lh, &maple_waitq) {
		mq = (struct mapleq *)lh;
		tmph = *lh;
		list_del(lh);
		list_add(lh, &maple_sentq);
		maple_build_block(mq);
		lh = &tmph;
		if (maple_packets++ > MAPLE_MAXPACKETS)
			break;
	}

	if (maple_packets>0) {
		for (i=0; i<(1<<MAPLE_DMA_PAGES); i++)
			dma_cache_wback_inv(maple_sendbuf+i*PAGE_SIZE, PAGE_SIZE);
		ctrl_outl(1, MAPLE_STATE);
	}

}


static void maple_timer_handler(unsigned long dummy)
{
	struct list_head *lh;
	struct maple_device *dev;

	/* If DMA is done, dispatch callback functions */
	maple_dma_irq();

	/* Do task for each device */
	list_for_each (lh, &maple_dev_list) {
		dev = (struct maple_device *)lh;
		if (dev->event)
			continue;
		if (dev->interval>0) {
			if (jiffies>dev->when) {
				dev->when = jiffies + dev->interval;
				dev->mq->command = MAPLE_COMMAND_GETCOND;
				dev->mq->sendbuf = &dev->function;
				dev->mq->length = 1;
				maple_add_packet(dev->mq);
			}
		}
		else {
			if (jiffies>=maple_pnp_time) {
				dev->mq->command = MAPLE_COMMAND_DEVINFO;
				dev->mq->length = 0;
				maple_add_packet(dev->mq);
			}
		}
	}

	if (jiffies>=maple_pnp_time)
		maple_pnp_time = jiffies + MAPLE_PNP_INTERVAL;
	
	/* Scan list and build sending block */
	maple_send();

	/* Schedule next timer event */
	mod_timer(&maple_timer, jiffies + MAPLE_SCANHZ);
}


static void maple_pnp_events(void)
{
	struct maple_device *dev;
	struct list_head *lh;

	list_for_each(lh, &maple_dev_list) {

		dev = (struct maple_device *)lh;

		switch(dev->event) {
		case MAPLE_RESPONSE_NONE:
			if (dev->driver)
				maple_detach_driver(dev);
			break;

		case MAPLE_RESPONSE_DEVINFO:
			if (!dev->driver)
				maple_attach_driver(dev);
			break;
		}

		dev->event = 0;

	}
}


static int kmapled_thread(void *hoge)
{
	lock_kernel();

	daemonize();

	strcpy(current->comm, "kmapled");

	do {
		maple_pnp_events();
		interruptible_sleep_on(&kmapled_wait);
	} while(!signal_pending(current));
	
	unlock_kernel();

	complete_and_exit(&kmapled_exited, 0);
}


static int __init maple_init(void)
{
	struct maple_device *dev;
	int i;

	printk(KERN_INFO "SEGA Dreamcast MAPLE Bus drivers\n");

	/* Allocate DMA buffer */
	maple_sendbuf = (void *)__get_dma_pages(GFP_KERNEL, MAPLE_DMA_PAGES);
	if (maple_sendbuf == NULL)
		return -ENOMEM;
	memset(maple_sendbuf, 0, MAPLE_DMA_SIZE);

	/* Register dummy maple driver */
	maple_register_driver(&maple_dummy_driver);

	/* Register basic port devices */
	for (i=0; i<MAPLE_PORTS; i++) {
		dev = maple_alloc_dev(i, 0);
		if (!dev)
			goto cleanup;
		maple_register_dev(dev);
	}

	/* Start kernel thread */
	kmapled_pid = kernel_thread(kmapled_thread, NULL,
				    CLONE_FS|CLONE_FILES|CLONE_SIGHAND);
	if (kmapled_pid==0)
		goto cleanup;

	/* Start to scan ports */
	maple_pnp_time = 0;

        /* Initialize hardware */
        ctrl_outl(MAPLE_MAGIC, MAPLE_RESET);
        ctrl_outl(0, MAPLE_RESET2);
        ctrl_outl(MAPLE_2MBPS|MAPLE_TIMEOUT(50000), MAPLE_SPEED);
	ctrl_outl(PHYSADDR(maple_sendbuf), MAPLE_DMAADDR);
        ctrl_outl(1, MAPLE_ENABLE);

	/* Initialize timer */
	init_timer(&maple_timer);
        maple_timer.expires = jiffies + MAPLE_SCANHZ;
        maple_timer.function = maple_timer_handler;
        add_timer(&maple_timer);

        return 0;

 cleanup:
	/* XXX: Must do proper clean-up */
	printk(KERN_INFO "maple: Register failed\n");
	return -ENOMEM;
}


static void __exit maple_exit(void)
{
	/* XXX: Must do proper clean-up */

	kill_proc(kmapled_pid, SIGTERM, 1);
	wait_for_completion(&kmapled_exited);

	if (maple_sendbuf)
		free_pages((unsigned long)maple_sendbuf, MAPLE_DMA_PAGES);

	del_timer(&maple_timer);
}


module_init(maple_init);
module_exit(maple_exit);

EXPORT_SYMBOL(maple_add_packet);
EXPORT_SYMBOL(maple_del_packet);
EXPORT_SYMBOL(maple_register_driver);
EXPORT_SYMBOL(maple_unregister_driver);
EXPORT_SYMBOL(maple_getcond_callback);

/*
 * Local variables:
 * c-basic-offset: 8
 * End:
 */
