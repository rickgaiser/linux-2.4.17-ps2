/*
 *  snsc_mpu300_siogpio.c : MPU-300 SuperIO GPIO driver
 *
 *  Copyright 2001,2002 Sony Corporation.
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
 */

#ifndef __KERNEL__
#error "this module must be compiled with -D__KERNEL__"
#endif

#include <linux/config.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>

#include <linux/delay.h>

#include <asm/io.h>

#include <errno.h>

#include <linux/snsc_ich_gpio.h>
#include <linux/snsc_mpu300_siogpio.h>

#ifdef MODULE
MODULE_AUTHOR("Sony NSC");
MODULE_DESCRIPTION("MPU-300 SuperIO GPIO driver");

#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif /* MODULE_LICENSE */
#endif /* MODULE */

#ifdef DEBUG
#define DEBUG_PRINT(args...)  printk(args)
#define DEBUG_PRINT_STATUS    __print_intr_status()
static void __print_intr_status( void );
#else
#define DEBUG_PRINT(args...)
#define DEBUG_PRINT_STATUS
#endif

#define PME_BASE  0x800

/* status registers */
#define SPIO_PME_STS     (PME_BASE + 0x00)
#define SPIO_PME_EN      (PME_BASE + 0x02)
#define SPIO_PME_STS1    (PME_BASE + 0x04)
#define SPIO_PME_STS2    (PME_BASE + 0x05)
#define SPIO_PME_STS3    (PME_BASE + 0x06)
#define SPIO_PME_STS4    (PME_BASE + 0x07)
#define SPIO_PME_STS5    (PME_BASE + 0x08)
#define SPIO_PME_EN1     (PME_BASE + 0x0a)
#define SPIO_PME_EN2     (PME_BASE + 0x0b)
#define SPIO_PME_EN3     (PME_BASE + 0x0c)
#define SPIO_PME_EN4     (PME_BASE + 0x0d)
#define SPIO_PME_EN5     (PME_BASE + 0x0e)
#define SPIO_SMI_STS1    (PME_BASE + 0x10)
#define SPIO_SMI_STS2    (PME_BASE + 0x11)
#define SPIO_SMI_STS3    (PME_BASE + 0x12)
#define SPIO_SMI_STS4    (PME_BASE + 0x13)
#define SPIO_SMI_STS5    (PME_BASE + 0x14)
#define SPIO_SMI_EN1     (PME_BASE + 0x16)
#define SPIO_SMI_EN2     (PME_BASE + 0x17)
#define SPIO_SMI_EN3     (PME_BASE + 0x18)
#define SPIO_SMI_EN4     (PME_BASE + 0x19)
#define SPIO_SMI_EN5     (PME_BASE + 0x1a)
#define SPIO_MSC_STS     (PME_BASE + 0x1c)

/* gpio mode & function registers */
#define SPIO_GP20        (PME_BASE + 0x2b)
#define SPIO_GP21        (PME_BASE + 0x2c)
#define SPIO_GP22        (PME_BASE + 0x2d)
#define SPIO_GP24        (PME_BASE + 0x2f)
#define SPIO_GP25        (PME_BASE + 0x30)
#define SPIO_GP26        (PME_BASE + 0x31)
#define SPIO_GP27        (PME_BASE + 0x32)

#define SPIO_GP34        (PME_BASE + 0x37)
#define SPIO_GP35        (PME_BASE + 0x38)
#define SPIO_GP36        (PME_BASE + 0x39)
#define SPIO_GP37        (PME_BASE + 0x3a)

#define SPIO_GP40        (PME_BASE + 0x3b)
#define SPIO_GP41        (PME_BASE + 0x3c)
#define SPIO_GP42        (PME_BASE + 0x3d)
#define SPIO_GP43        (PME_BASE + 0x3e)

#define SPIO_GP60        (PME_BASE + 0x47)
#define SPIO_GP61        (PME_BASE + 0x48)

/* gpio data registers */
#define SPIO_GP2         (PME_BASE + 0x4c)
#define SPIO_GP3         (PME_BASE + 0x4d)
#define SPIO_GP4         (PME_BASE + 0x4e)
#define SPIO_GP6         (PME_BASE + 0x50)


/* ICH2 GPIO[6] */
#define ICHGPIO_LPC_PME  6


/* GPIO mode bitmask */
#define MODE_INOUT    (1 << 0)
#define MODE_POLARITY (1 << 1)
#define MODE_OUTTYPE  (1 << 7)
#define MODE_EETI     (2 << 2)
#define MODE_EETI_43  (3 << 2)


#define INTR_DEV_ID   gpio

#define NUM_GPIO  16
#define MAX_INTR_HANDLERS  10

#define IS_BIT_SET(bits, x)  ((bits) & (1 << (x)))
#define BIT_NUM_CHECK(bit_num)  bit_num = ((bit_num) % NUM_GPIO)


static const char driver_name[] = "snsc_mpu300_siogpio";

static struct _gpio_pin_property {
	int name;
	unsigned int mode_reg;
	unsigned int data_reg;
	__u8 data_mask;
	unsigned int intr_reg;
	unsigned int status_reg;
	__u8 pme_mask;
	int intr_capable;
	int eeti_capable;
} gpio[NUM_GPIO] = {
  { 21, SPIO_GP21, SPIO_GP2, 0x02, SPIO_PME_EN3, SPIO_PME_STS3, 0x02, 1, 1 },
  { 22, SPIO_GP22, SPIO_GP2, 0x04, SPIO_PME_EN3, SPIO_PME_STS3, 0x04, 1, 1 },
  { 41, SPIO_GP41, SPIO_GP4, 0x02, SPIO_PME_EN4, SPIO_PME_STS4, 0x10, 1, 1 },
  { 43, SPIO_GP43, SPIO_GP4, 0x08, SPIO_PME_EN4, SPIO_PME_STS4, 0x20, 1, 1 },
  { 60, SPIO_GP60, SPIO_GP6, 0x01, SPIO_PME_EN4, SPIO_PME_STS4, 0x40, 1, 1 },
  { 61, SPIO_GP61, SPIO_GP6, 0x02, SPIO_PME_EN4, SPIO_PME_STS4, 0x80, 1, 1 },
  { 20, SPIO_GP20, SPIO_GP2, 0x01, SPIO_PME_EN3, SPIO_PME_STS3, 0x01, 1, 0 }, 
  { 25, SPIO_GP25, SPIO_GP2, 0x20, SPIO_PME_EN3, SPIO_PME_STS3, 0x20, 1, 0 },
  { 26, SPIO_GP26, SPIO_GP2, 0x40, SPIO_PME_EN3, SPIO_PME_STS3, 0x40, 1, 0 },
  { 27, SPIO_GP27, SPIO_GP2, 0x80, SPIO_PME_EN3, SPIO_PME_STS3, 0x80, 1, 0 },
  { 34, SPIO_GP34, SPIO_GP3, 0x10 },
  { 35, SPIO_GP35, SPIO_GP3, 0x20 },
  { 36, SPIO_GP36, SPIO_GP3, 0x40 },
  { 37, SPIO_GP37, SPIO_GP3, 0x80 },
  { 40, SPIO_GP40, SPIO_GP4, 0x01 },
  { 24, SPIO_GP24, SPIO_GP2, 0x10 }
};

static struct gpio_intr_handler {
	__u16 bits;
	void (*handler)(int irq, void *dev_id, struct pt_regs *regs);
	void *dev_id;
} intr_handlers[MAX_INTR_HANDLERS];

static int num_intr_handlers;

static spinlock_t  siogpio_write_lock;
static spinlock_t  siogpio_intr_lock;



/*
 * Initialize function
 */
int mpu300_siogpio_enable(__u16 bits, __u32 mode)
{
	int i;
	__u8 conf = 0;

	/* bit 15 cannot be used for input */
	if ((mode & SIOGPIO_MODE_IN) && (IS_BIT_SET(bits, 15)))
		return -EINVAL;

	if ( mode & SIOGPIO_MODE_IN ) conf |= MODE_INOUT;
	if ( mode & SIOGPIO_MODE_INVERT ) conf |= MODE_POLARITY;
	if ( mode & SIOGPIO_MODE_OPENDRAIN ) conf |= MODE_OUTTYPE;

	for ( i = 0 ; i < NUM_GPIO; i++){
		if (IS_BIT_SET(bits, i)){
			mpu300_siogpio_intr_mask_bit(i);
			outb_p(conf, gpio[i].mode_reg);
			//udelay(50); /* for clear interrupt status */
			mpu300_siogpio_intr_clear_bit(i);
		}
	}

	return 0;
}


/*
 * Data register access functions
 */
static inline int __get_data_bit(int bit_num)
{
	return (inb(gpio[bit_num].data_reg) & gpio[bit_num].data_mask) ? 1 : 0;
}

int mpu300_siogpio_get_data_bit(int bit_num)
{
	BIT_NUM_CHECK(bit_num);

	return __get_data_bit(bit_num);
}

__u16 mpu300_siogpio_get_data(__u16 bits)
{
	__u16 data;
	__u8 gp2, gp3, gp4, gp6;

	gp2 = inb(SPIO_GP2);
	gp3 = inb(SPIO_GP3);
	gp4 = inb(SPIO_GP4);
	gp6 = inb(SPIO_GP6);

	data =
		((gp2 & 0x06) >> 1) |
		((gp4 & 0x02) >> 1) <<  2 |
		((gp4 & 0x08) >> 3) <<  3 |
		((gp6 & 0x03) >> 0) <<  4 |
		((gp2 & 0x01) >> 0) <<  6 |
		((gp2 & 0xe0) >> 5) <<  7 |
		((gp3 & 0xf0) >> 4) << 10 |
		((gp4 & 0x01) >> 0) << 14 |
		((gp2 & 0x10) >> 4) << 15;

	data &= bits;

	/* general (but slightly inefficient) code */
	/*
	int i;
	__u16 data = 0;

	for (i = 0; i < NUM_GPIO; i++){
		if (IS_BIT_SET(bits, i)){
			data |= __get_data_bit(i) << i;
		}
	}
	*/

	return data;
}


/*
 * Internal set_data_bit function.
 * This function must be called with 'write_lock' owned.
 */
static void __set_data_bit(int bit_num, int value)
{
	__u8 data;

	data = inb(gpio[bit_num].data_reg);

	if (value)
		data |= gpio[bit_num].data_mask;
	else
		data &= ~gpio[bit_num].data_mask;

	outb_p(data, gpio[bit_num].data_reg);
}

void mpu300_siogpio_set_data_bit(int bit_num, int value)
{
	unsigned long flags;

	spin_lock_irqsave(&siogpio_write_lock, flags);

	BIT_NUM_CHECK(bit_num);
 
	__set_data_bit(bit_num, value);

	spin_unlock_irqrestore(&siogpio_write_lock, flags);
}

void mpu300_siogpio_set_data(__u16 bits, __u16 value)
{
	__u8 gp2, gp3, gp4, gp6;
	unsigned long flags;

	spin_lock_irqsave(&siogpio_write_lock ,flags);

	value &= bits;
	value |= mpu300_siogpio_get_data(SIOGPIO_BITS_ALL) & ~bits;

	gp2 = 
		((value & 0x0040) >>  6) |
		((value & 0x0003) >>  0) << 1 |
		((value & 0x8000) >> 15) << 4 |
		((value & 0x0380) >>  7) << 5;

	gp3 = 
		((value & 0x3c00) >> 10 ) << 4;

	gp4 =
		((value & 0x4000) >> 14) |
		((value & 0x0004) >>  2) << 1 |
		((value & 0x0008) >>  3) << 3;

	gp6 =
		((value & 0x0030) >> 4);

	outb_p(gp2, SPIO_GP2);
	outb_p(gp3, SPIO_GP3);
	outb_p(gp4, SPIO_GP4);
	outb_p(gp6, SPIO_GP6);

	spin_unlock_irqrestore(&siogpio_write_lock, flags);

	/* general (but slightly inefficient) code */
	/*
	int i;
	unsigned long flags;
  
	spin_locl_irqsave(&siogpio_write_lock, flags);
	for (i = 0;  i < NUM_GPIO; i++){
		if (IS_BIT_SET(bits, i)){
			__set_data_bit(i, IS_BIT_SET(value, i));
		}
	}
	spin_unlock_irqrestore(&siogpio_write_lock, flags);
	*/
}



/*
 * Interrupt handling functions
 */
void mpu300_siogpio_intr_enable( void )
{
	//outb_p(inb(SPIO_PME_EN) | 0x01, SPIO_PME_EN);
	ichgpio_enable_intr(ICHGPIO_LPC_PME);
}


void mpu300_siogpio_intr_disable( void )
{
	//outb_p(inb(SPIO_PME_EN) & ~0x01, SPIO_PME_EN);
	ichgpio_disable_intr(ICHGPIO_LPC_PME);
}


int mpu300_siogpio_intr_register_handler(__u16 bits, __u32 mode,
					 void (*handler)(int irq, 
							 void *dev_id,
							 struct pt_regs *regs),
					 void *dev_id)
{
	int i;
	int ret = 0;
	unsigned long flags;
	__u8 conf;

	for (i = 0; i < NUM_GPIO; i++){
		if (IS_BIT_SET(bits, i)){
			if (!gpio[i].intr_capable ||
			    ((mode == SIOGPIO_INTR_BOTH_EDGE) && (!gpio[i].eeti_capable))){
				return  -EINVAL;
			}
		}
	}

	spin_lock_irqsave(&siogpio_intr_lock, flags);

	for (i = 0; i < num_intr_handlers; i++){
		if (intr_handlers[i].bits & bits){
			ret = -EBUSY;
			goto reg_handler_exit;
		}
	}

	for (i = 0; i < NUM_GPIO; i++){
		if (IS_BIT_SET(bits, i)){
			conf = inb(gpio[i].mode_reg);
			switch (mode){
			case SIOGPIO_INTR_POSITIVE_EDGE:
				conf &= ~MODE_POLARITY;
				break;
			case SIOGPIO_INTR_NEGATIVE_EDGE:
				conf |= MODE_POLARITY;
				break;
			case SIOGPIO_INTR_BOTH_EDGE:
				conf |= (gpio[i].name == 43) ? 
					MODE_EETI_43 : MODE_EETI;
				break;
			default:
				ret = -EINVAL;
				goto reg_handler_exit;
			}
			outb_p(conf, gpio[i].mode_reg);
		}
	}

	intr_handlers[num_intr_handlers].bits = bits;
	intr_handlers[num_intr_handlers].handler = handler;
	intr_handlers[num_intr_handlers].dev_id = dev_id;

	num_intr_handlers++;

#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif /* MODULE */

reg_handler_exit:

	spin_unlock_irqrestore(&siogpio_intr_lock, flags);
	return ret;
}


void mpu300_siogpio_intr_unregister_handler(__u16 bits)
{
	int i, j;
	unsigned long flags;
	__u16 new_bits;

	mpu300_siogpio_intr_mask(bits);

	spin_lock_irqsave(&siogpio_intr_lock, flags);

	for (i = 0, j = 0; i < num_intr_handlers; i++){
		new_bits = intr_handlers[i].bits & ~bits;
		if (new_bits) {
			intr_handlers[j].bits = new_bits;
			intr_handlers[j].handler = intr_handlers[i].handler;
			intr_handlers[j].dev_id = intr_handlers[i].dev_id;
			j++;
		}
	}
#ifdef MODULE
	for (i = 0; i < num_intr_handlers - j; i++){
		MOD_DEC_USE_COUNT;
  }
#endif /* MODULE */

	num_intr_handlers = j;

	spin_unlock_irqrestore(&siogpio_intr_lock, flags);

	return;
}



#define SIOGPIO_INTR_ENABLE  1
#define SIOGPIO_INTR_DISABLE 0

static void __intr_change_mask_bit(int bit_num, int enable)
{
	__u8 mask;
	int i;
	unsigned long flags;

	BIT_NUM_CHECK(bit_num);

	if(!gpio[bit_num].intr_reg)
		return;

	spin_lock_irqsave(&siogpio_intr_lock, flags);

	if (enable == SIOGPIO_INTR_ENABLE){
		for (i = 0; i < num_intr_handlers; i++){
			if (IS_BIT_SET(intr_handlers[i].bits, bit_num))
				break;
		}
		if (i == num_intr_handlers)
			goto change_mask_bit_exit;
	}

	mask = inb(gpio[bit_num].intr_reg);

	if (enable == SIOGPIO_INTR_ENABLE)
		mask |= gpio[bit_num].pme_mask;
	else
		mask &= ~gpio[bit_num].pme_mask;

	outb_p(mask, gpio[bit_num].intr_reg);

change_mask_bit_exit:

	spin_unlock_irqrestore(&siogpio_intr_lock, flags);
}


void mpu300_siogpio_intr_unmask_bit(int bit_num)
{
	BIT_NUM_CHECK(bit_num);

	return __intr_change_mask_bit(bit_num, SIOGPIO_INTR_ENABLE);
}

void mpu300_siogpio_intr_mask_bit(int bit_num)
{
	BIT_NUM_CHECK(bit_num);

	return __intr_change_mask_bit(bit_num, SIOGPIO_INTR_DISABLE);
}


static void __intr_change_mask(__u16 bits, int enable)
{
	__u16 en_mask = 0;
	__u8 mask_lb, mask_hb;
	__u8 en3, en4;
	int i;
	unsigned long flags;

	spin_lock_irqsave(&siogpio_intr_lock, flags);

	if (enable == SIOGPIO_INTR_ENABLE){
		for (i = 0; i < num_intr_handlers; i++){
			en_mask |= intr_handlers[i].bits;
		}
		bits &= en_mask;
	}

	mask_lb =
		((bits & 0x0003) >> 0) << 1 |
		((bits & 0x0040) >> 6) |
		((bits & 0x0380) >> 7) << 5;

	mask_hb =
		((bits & 0x003c) >> 2) << 4;

	en3 = inb(SPIO_PME_EN3);
	en4 = inb(SPIO_PME_EN4);

	if (enable == SIOGPIO_INTR_ENABLE){
		en3 |= mask_lb;
		en4 |= mask_hb;
	} else {
		en3 &= ~mask_lb;
		en4 &= ~mask_hb;
	}

	outb_p(en3, SPIO_PME_EN3);
	outb_p(en4, SPIO_PME_EN4);

	spin_unlock_irqrestore(&siogpio_intr_lock, flags);  
}

void mpu300_siogpio_intr_unmask(__u16 bits)
{
	return __intr_change_mask(bits, SIOGPIO_INTR_ENABLE);
}

void mpu300_siogpio_intr_mask(__u16 bits)
{
	return __intr_change_mask(bits, SIOGPIO_INTR_DISABLE);
}



static inline int __get_status_bit(int bit_num)
{
	if (!gpio[bit_num].status_reg)
		return 0;

	return (inb(gpio[bit_num].status_reg) & gpio[bit_num].pme_mask) ? 1 : 0;
}

int mpu300_siogpio_intr_get_status_bit(int bit_num)
{
	BIT_NUM_CHECK(bit_num);

	return __get_status_bit(bit_num);
}

__u16 mpu300_siogpio_intr_get_status(__u16 bits)
{
	__u16 status;
	__u8 sts3, sts4;

	sts3 = inb(SPIO_PME_STS3);
	sts4 = inb(SPIO_PME_STS4);

	status =
		((sts3 & 0x06) >> 1) |
		((sts4 & 0xf0) >> 4) << 2 |
		((sts3 & 0x01) >> 0) << 6 |
		((sts3 & 0xe0) >> 5) << 7 ;

	status &= bits;

	/* general (but slightly inefficient) code */
	/*
	int i;
	__u16 status = 0;

	for (i = 0; i < NUM_GPIO; i++){
		if (IS_BIT_SET(bits, i)){
			data |= __get_status_bit(i) << i;
		}
	}
	*/

	return status;
}

void  mpu300_siogpio_intr_clear_bit(int bit_num)
{
	BIT_NUM_CHECK(bit_num);

	if (!gpio[bit_num].status_reg)
		return;

	outb_p(gpio[bit_num].pme_mask, gpio[bit_num].status_reg);
}

void  mpu300_siogpio_intr_clear( void )
{
	outb_p(1, SPIO_PME_STS);

	ichgpio_clear_intr(ICHGPIO_LPC_PME);
}



/*
 * interrupt handler
 */
void __intr_handler(int irq, void *dev_id, struct pt_regs *pt_regs)
{
	int i;
	__u16 intr_status;

	DEBUG_PRINT(KERN_INFO "%s: Interrupt occured,\n", driver_name);
	DEBUG_PRINT_STATUS;

	if (!ichgpio_int_status(ICHGPIO_LPC_PME)){
		DEBUG_PRINT(KERN_INFO "this interrupt is not caused by SuperIO\n");
		return;
	}

	if (dev_id != INTR_DEV_ID){
		DEBUG_PRINT(KERN_INFO "wrong dev_id...\n");
		return;
	}

	intr_status = mpu300_siogpio_intr_get_status(SIOGPIO_BITS_ALL);

	for (i = 0; i < num_intr_handlers; i++){
		if (intr_status & intr_handlers[i].bits)
			intr_handlers[i].handler(irq, intr_handlers[i].dev_id, pt_regs);
	}

	mpu300_siogpio_intr_clear();

	ichgpio_clear_intr(ICHGPIO_LPC_PME);

	DEBUG_PRINT(KERN_INFO "%s: Interrupt handler end,\n", driver_name);
	DEBUG_PRINT_STATUS;

}

#ifdef DEBUG
static void __print_intr_status( void )
{
	DEBUG_PRINT("PME_STS1=%02x STS2=%02x STS3=%02x STS4=%02x STS5=%02x, status=%02x\n",
		    inb(SPIO_PME_STS1), inb(SPIO_PME_STS2), inb(SPIO_PME_STS3),
		    inb(SPIO_PME_STS4), inb(SPIO_PME_STS5),
		    mpu300_siogpio_intr_get_status(SIOGPIO_BITS_ALL));
	DEBUG_PRINT(" PME_EN1=%02x  EN2=%02x  EN3=%02x  EN4=%02x  EN5=%02x\n",
		    inb(SPIO_PME_EN1), inb(SPIO_PME_EN2), inb(SPIO_PME_EN3),
		    inb(SPIO_PME_EN4), inb(SPIO_PME_EN5));
	DEBUG_PRINT("PME_STS=%02x PME_EN=%02x ichgpio=%0x\n",
		    inb(SPIO_PME_STS), inb(SPIO_PME_EN),
		    ichgpio_int_status(ICHGPIO_LPC_PME));

}
#endif

/*
 * setup / cleanup functions
 */
static int mpu300_siogpio_init( void )
{
	int res;
	int i;

	spin_lock_init(&siogpio_write_lock);
	spin_lock_init(&siogpio_intr_lock);

	/* Disable interrupt and mask all bit */
	mpu300_siogpio_intr_disable();
	mpu300_siogpio_intr_mask(SIOGPIO_BITS_ALL);
	/*
	outb_p(0, SPIO_PME_EN1);
	outb_p(0, SPIO_PME_EN2);
	outb_p(0, SPIO_PME_EN3);
	outb_p(0, SPIO_PME_EN4);
	outb_p(0, SPIO_PME_EN5);
	*/

	mpu300_siogpio_enable(0x00ff, SIOGPIO_MODE_IN | SIOGPIO_MODE_NOINVERT);
	mpu300_siogpio_enable(0xff00, SIOGPIO_MODE_OUT | SIOGPIO_MODE_NOINVERT | SIOGPIO_MODE_PUSHPULL);

	mpu300_siogpio_set_data(SIOGPIO_BITS_ALL, 0);

	/* Interrupt status bit clear */
	for (i = 0; i < NUM_GPIO; i++){
		mpu300_siogpio_intr_clear_bit(i);
	}
	/*
	outb_p(0xff, SPIO_PME_STS1);
	outb_p(0xff, SPIO_PME_STS2);
	outb_p(0xff, SPIO_PME_STS3);
	outb_p(0xff, SPIO_PME_STS4);
	outb_p(0xff, SPIO_PME_STS5);
	*/
	mpu300_siogpio_intr_clear();

	/* enable nIO_PME output (active low) */
	outb_p(0x04, SPIO_GP42);

	/* enable nIO_PME signal assertion */
	outb_p(inb(SPIO_PME_EN) | 0x01, SPIO_PME_EN);

	/* ICH2 GPIO setup */
	ichgpio_disable_intr(ICHGPIO_LPC_PME);
	ichgpio_use_sel(ICHGPIO_LPC_PME, ICHGPIO_USESEL_GPIO);
	ichgpio_sig_inv(ICHGPIO_LPC_PME, ICHGPIO_SIGINV_LOW);
	ichgpio_ctrl_rout(ICHGPIO_LPC_PME, ICHGPIO_CTRLROUT_SCI);
	ichgpio_clear_intr(ICHGPIO_LPC_PME);

	res = request_irq(ichgpio_irq(), __intr_handler, 
			  SA_SHIRQ, "siogpio", INTR_DEV_ID);

	if (res){
		printk(KERN_INFO "%s: can't install interrupt handler, irq=%d\n",
		       driver_name, ichgpio_irq());
		return -1;
	}

	printk(KERN_INFO "%s: installed interrupt handler, irq=%d\n",
	       driver_name, ichgpio_irq());

	DEBUG_PRINT_STATUS;

	ichgpio_enable_intr(ICHGPIO_LPC_PME);
	mpu300_siogpio_intr_enable();

	return 0;
}

static void mpu300_siogpio_cleanup( void )
{
	int i;

	ichgpio_disable_intr(ICHGPIO_LPC_PME);
	outb_p(inb(SPIO_PME_EN) & ~0x01, SPIO_PME_EN);

	for ( i = 0; i < NUM_GPIO; i++){
		mpu300_siogpio_intr_clear_bit(i);
	}
	mpu300_siogpio_intr_clear();
	ichgpio_clear_intr(ICHGPIO_LPC_PME);

	free_irq(ichgpio_irq(), INTR_DEV_ID);
}

#ifdef MODULE
static int mpu300_siogpio_init_module( void )
{
	printk(KERN_INFO "%s: SuperIO GPIO driver loaded.\n", driver_name);
	return mpu300_siogpio_init();
}

static void mpu300_siogpio_cleanup_module( void )
{
	printk(KERN_INFO "%s: SuperIO GPIO driver unloaded.\n", driver_name);
	mpu300_siogpio_cleanup();
}
#endif /* MODULE */

#ifndef MODULE
__initcall(mpu300_siogpio_init);
__exitcall(mpu300_siogpio_cleanup);
#else /* MODULE */
module_init(mpu300_siogpio_init_module);
module_exit(mpu300_siogpio_cleanup_module);
#endif /* MODULE */

EXPORT_SYMBOL(mpu300_siogpio_enable);
EXPORT_SYMBOL(mpu300_siogpio_get_data_bit);
EXPORT_SYMBOL(mpu300_siogpio_set_data_bit);
EXPORT_SYMBOL(mpu300_siogpio_get_data);
EXPORT_SYMBOL(mpu300_siogpio_set_data);
EXPORT_SYMBOL(mpu300_siogpio_intr_enable);
EXPORT_SYMBOL(mpu300_siogpio_intr_disable);
EXPORT_SYMBOL(mpu300_siogpio_intr_register_handler);
EXPORT_SYMBOL(mpu300_siogpio_intr_unregister_handler);
EXPORT_SYMBOL(mpu300_siogpio_intr_unmask_bit);
EXPORT_SYMBOL(mpu300_siogpio_intr_unmask);
EXPORT_SYMBOL(mpu300_siogpio_intr_mask_bit);
EXPORT_SYMBOL(mpu300_siogpio_intr_mask);
EXPORT_SYMBOL(mpu300_siogpio_intr_get_status_bit);
EXPORT_SYMBOL(mpu300_siogpio_intr_get_status);
EXPORT_SYMBOL(mpu300_siogpio_intr_clear_bit);
EXPORT_SYMBOL(mpu300_siogpio_intr_clear);

