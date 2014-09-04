/*
 * i2c-algo-8260.c i2c driver algorithms for 8260 CPM
 * Copyright (c) 1999 Dan Malek (dmalek@jlc.net).
 *
 * moved into proper i2c interface; separated out platform specific
 * parts into ic-rpx.c
 * Brad Parker (brad@heeltoe.com)
 *
 * Copied this from the 8xx version -  Dan
 */

// XXX todo
// timeout sleep?

/* $Id: i2c-algo-8260.c,v 1.2 2002/01/11 00:17:01 alowe Exp $ */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <linux/ioport.h>
#include <linux/errno.h>
#include <linux/sched.h>

#include <asm/mpc8260.h>
#include <asm/cpm_8260.h>
#include <asm/irq.h>

#include <linux/i2c.h>
#include <linux/i2c-algo-8260.h>

#define CPM_MAX_READ	513

static wait_queue_head_t iic_wait;
static ushort r_tbase, r_rbase;

int cpm_scan = 1;
#define cpm_debug  0	/* This makes compiler remove the code */

static void
cpm_iic_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	volatile i2c8260_t *i2c = (i2c8260_t *)dev_id;

	if (cpm_debug > 1)
		printk("cpm_iic_interrupt(dev_id=%p)\n", dev_id);

#if 0
	/* Clear enable.
	*/
	i2c->i2c_i2mod = 0;
#endif

	/* Clear interrupt.
	*/
	i2c->i2c_i2cer = 0xff;

	/* Get 'me going again.
	*/
	wake_up_interruptible(&iic_wait);
}

static void
cpm_iic_init(struct i2c_algo_8260_data *cpm_adap)
{
	volatile iic_t		*iip = cpm_adap->iip;
	volatile i2c8260_t	*i2c = cpm_adap->i2c;

	if (cpm_debug) printk("cpm_iic_init()\n");

	if (cpm_adap->reloc != 0) {
		/* Initialize the parameter ram.
		 * We need to make sure many things are initialized to zero,
		 * especially in the case of a microcode patch.
		 */
		iip->iic_rstate = 0;
		iip->iic_rdp = 0;
		iip->iic_rbptr = 0;
		iip->iic_rbc = 0;
		iip->iic_rxtmp = 0;
		iip->iic_tstate = 0;
		iip->iic_tdp = 0;
		iip->iic_tbptr = 0;
		iip->iic_tbc = 0;
		iip->iic_txtmp = 0;
	}

	/* Set up the IIC parameters in the parameter ram.
	*/
	iip->iic_tbase = r_tbase = cpm_adap->dp_addr;
	iip->iic_rbase = r_rbase = cpm_adap->dp_addr + sizeof(cbd_t)*2;

	iip->iic_tfcr = (CPMFCR_GBL | CPMFCR_EB);
	iip->iic_rfcr = (CPMFCR_GBL | CPMFCR_EB);

	/* Set maximum receive size.
	*/
	iip->iic_mrblr = CPM_MAX_READ;

	/* Initialize Tx/Rx parameters.
	*/
	if (cpm_adap->reloc == 0) {
		volatile cpm8260_t *cp = cpm_adap->cp;

		cp->cp_cpcr =
			mk_cr_cmd(CPM_CR_I2C_PAGE, CPM_CR_I2C_SBLOCK, 0,
						CPM_CR_INIT_TRX) | CPM_CR_FLG;
		while (cp->cp_cpcr & CPM_CR_FLG);
	}

	/* Select an arbitrary address.  Just make sure it is unique.
	*/
	i2c->i2c_i2add = 0x34;

	/* Make clock run almost maximum slow.
	*/
	i2c->i2c_i2brg = 7;

	/* Disable interrupts.
	*/
	i2c->i2c_i2cmr = 0;
	i2c->i2c_i2cer = 0xff;

	init_waitqueue_head(&iic_wait);

	/* Install interrupt handler.
	*/
	request_8xxirq(SIU_INT_I2C, cpm_iic_interrupt, 0, "i2c", (void *)i2c);

	i2c->i2c_i2mod = 0x19;
}


static int
cpm_iic_shutdown(struct i2c_algo_8260_data *cpm_adap)
{
	volatile i2c8260_t *i2c = cpm_adap->i2c;

	/* Shut down IIC.
	*/
	i2c->i2c_i2mod = 0;
	i2c->i2c_i2cmr = 0;
	i2c->i2c_i2cer = 0xff;

	return(0);
}

#define BD_SC_NAK		((ushort)0x0004) /* NAK - did not respond */
#define CPM_CR_CLOSE_RXBD	((ushort)0x0007)

static void force_close(struct i2c_algo_8260_data *cpm)
{
	if (cpm->reloc == 0) {
		volatile cpm8260_t *cp = cpm->cp;

		if (cpm_debug) printk("force_close()\n");
		cp->cp_cpcr =
			mk_cr_cmd(CPM_CR_I2C_PAGE, CPM_CR_I2C_SBLOCK, 0,
					CPM_CR_CLOSE_RXBD) | CPM_CR_FLG;

		while (cp->cp_cpcr & CPM_CR_FLG);
	}
}


/* Read from IIC...
 * abyte = address byte, with r/w flag already set
 */
static int
cpm_iic_read(struct i2c_algo_8260_data *cpm, u_char abyte, char *buf, int count)
{
	volatile iic_t *iip = cpm->iip;
	volatile i2c8260_t *i2c = cpm->i2c;
	volatile cbd_t	*tbdf, *rbdf;
	u_char *tb;
	unsigned long flags;
	volatile immap_t	*immap;
	int	i;

	if (count >= CPM_MAX_READ)
		return -EINVAL;


	immap = (immap_t *)IMAP_ADDR;	/* and to internal registers */
	tbdf = (cbd_t *)&immap->im_dprambase[iip->iic_tbase];
	rbdf = (cbd_t *)&immap->im_dprambase[iip->iic_rbase];

	/* To read, we need an empty buffer of the proper length.
	 * All that is used is the first byte for address, the remainder
	 * is just used for timing (and doesn't really have to exist).
	 */
	tb = cpm->temp;
	tb = (u_char *)(((uint)tb + 15) & ~15);
	tb[0] = abyte;		/* Device address byte w/rw flag */

	if (cpm_debug) printk("cpm_iic_read(abyte=0x%x)\n", abyte);

	tbdf->cbd_bufaddr = __pa(tb);
	tbdf->cbd_datlen = count + 1;
	tbdf->cbd_sc =
		BD_SC_READY | BD_SC_INTRPT | BD_SC_LAST |
		BD_SC_WRAP | BD_IIC_START;

	rbdf->cbd_datlen = 0;
	rbdf->cbd_bufaddr = __pa(buf);
	rbdf->cbd_sc = BD_SC_EMPTY | BD_SC_WRAP;

	/* Chip bug?  It was on 8xx, doing set enable here
	 * works better.
	 */
	save_flags(flags); cli();
	i2c->i2c_i2cer = 0xff;
	i2c->i2c_i2cmr = 0x13;	/* Enable some interupts */
#if 0
	i2c->i2c_i2mod = 1;	/* Enable */
#endif
	i2c->i2c_i2com = 0x81;	/* Start master */

	/* Wait for IIC transfer */
	interruptible_sleep_on(&iic_wait);
	restore_flags(flags);
	if (signal_pending(current))
		return -EIO;

	if (cpm_debug) {
		printk("tx sc %04x, rx sc %04x\n",
		       tbdf->cbd_sc, rbdf->cbd_sc);
	}

#if 0
	if (tbdf->cbd_sc & BD_SC_NAK) {
		printk("IIC read; no ack\n");
		return 0;
	}
#else
	/* I don't get it.....with the debug enabled (so the message
	 * above is printed), everything works fine.  For some reason,
	 * we need just a small delay (after all of the overhead of
	 * interrupt/schedule) to ensure NAK is cleared.  It is some
	 * CPM timing problem I guess I don't understand.  The CPM
	 * must set this at the start of the transfer, and then clear it
	 * later.  We just win the race (that we don't want to win).
	 * When the device is present, I always see i == 0.....  -- Dan.
	 */
	for (i=0; i<10; i++) {
		if ((tbdf->cbd_sc & BD_SC_NAK) == 0)
			break;
		mdelay(30);
	}
	if (i >= 10) {
		if (cpm_debug) printk("IIC read; no ack\n");
		return 0;
	}
#endif

	if (rbdf->cbd_sc & BD_SC_EMPTY) {
		printk("IIC read; complete but rbuf empty\n");
		force_close(cpm);
		printk("tx sc %04x, rx sc %04x\n",
		       tbdf->cbd_sc, rbdf->cbd_sc);
	}

	if (cpm_debug) printk("read %d bytes\n", rbdf->cbd_datlen);

	if (rbdf->cbd_datlen < count) {
		printk("IIC read; short, wanted %d got %d\n",
		       count, rbdf->cbd_datlen);
		return 0;
	}

	return count;
}

/* Write to IIC...
 * addr = address byte, with r/w flag already set
 */
static int
cpm_iic_write(struct i2c_algo_8260_data *cpm, u_char abyte, char *buf,int count)
{
	volatile iic_t *iip = cpm->iip;
	volatile i2c8260_t *i2c = cpm->i2c;
	volatile cbd_t	*tbdf;
	volatile immap_t	*immap;
	u_char *tb;
	unsigned long flags;

	immap = (immap_t *)IMAP_ADDR;	/* and to internal registers */
	tb = cpm->temp;
	tb = (u_char *)(((uint)tb + 15) & ~15);
	*tb = abyte;		/* Device address byte w/rw flag */

	if (cpm_debug) printk("cpm_iic_write(abyte=0x%x)\n", abyte);

	/* set up 2 descriptors */
	tbdf = (cbd_t *)&immap->im_dprambase[iip->iic_tbase];

	tbdf[0].cbd_bufaddr = __pa(tb);
	tbdf[0].cbd_datlen = 1;
	tbdf[0].cbd_sc = BD_SC_READY | BD_IIC_START;

	tbdf[1].cbd_bufaddr = __pa(buf);
	tbdf[1].cbd_datlen = count;
	tbdf[1].cbd_sc = BD_SC_READY | BD_SC_INTRPT | BD_SC_LAST | BD_SC_WRAP;

	/* Chip bug, set enable here */
	save_flags(flags); cli();
	i2c->i2c_i2cer = 0xff;
	i2c->i2c_i2cmr = 0x13;	/* Enable some interupts */
#if 0
	i2c->i2c_i2mod = 1;	/* Enable */
#endif
	i2c->i2c_i2com = 0x81;	/* Start master */

	/* Wait for IIC transfer */
	interruptible_sleep_on(&iic_wait);
	restore_flags(flags);
	if (signal_pending(current))
		return -EIO;

	if (cpm_debug) {
		printk("tx0 sc %04x, tx1 sc %04x\n",
		       tbdf[0].cbd_sc, tbdf[1].cbd_sc);
	}

	mdelay(30);
	if (tbdf->cbd_sc & BD_SC_NAK) {
		printk("IIC write; no ack\n");
		return 0;
	}
	
	if (tbdf->cbd_sc & BD_SC_READY) {
		printk("IIC write; complete but tbuf ready\n");
		return 0;
	}

	return count;
}

/* See if an IIC address exists..
 * addr = 7 bit address, unshifted
 */
static int
cpm_iic_tryaddress(struct i2c_algo_8260_data *cpm, int addr)
{
	volatile iic_t *iip = cpm->iip;
	volatile i2c8260_t *i2c = cpm->i2c;
	volatile cbd_t *tbdf, *rbdf;
	volatile immap_t	*immap;
	u_char *tb;
	unsigned long flags, len;

	if (cpm_debug > 1)
		printk("cpm_iic_tryaddress(cpm=%p,addr=%d)\n", cpm, addr);

	if (cpm_debug && addr == 0) {
		printk("iip %p, dp_addr 0x%x\n", cpm->iip, cpm->dp_addr);
		printk("iic_tbase %d, r_tbase %d\n", iip->iic_tbase, r_tbase);
	}

	immap = (immap_t *)IMAP_ADDR;	/* and to internal registers */
	tbdf = (cbd_t *)&immap->im_dprambase[iip->iic_tbase];
	rbdf = (cbd_t *)&immap->im_dprambase[iip->iic_rbase];

	tb = cpm->temp;
	tb = (u_char *)(((uint)tb + 15) & ~15);

	/* do a simple read */
	tb[0] = (addr << 1) | 1;	/* device address (+ read) */
	len = 2;

	tbdf->cbd_bufaddr = __pa(tb);
	tbdf->cbd_datlen = len;
	tbdf->cbd_sc =
		BD_SC_READY | BD_SC_INTRPT | BD_SC_LAST |
		BD_SC_WRAP | BD_IIC_START;

	rbdf->cbd_datlen = 0;
	rbdf->cbd_bufaddr = __pa(tb+2);
	rbdf->cbd_sc = BD_SC_EMPTY | BD_SC_WRAP;

	save_flags(flags); cli();
	i2c->i2c_i2cer = 0xff;
	i2c->i2c_i2cmr = 0x13;	/* Enable some interupts */
#if 0
	i2c->i2c_i2mod = 1;	/* Enable */
#endif
	i2c->i2c_i2com = 0x81;	/* Start master */

	if (cpm_debug > 1) printk("about to sleep\n");

	/* wait for IIC transfer */
	interruptible_sleep_on(&iic_wait);
	restore_flags(flags);
	if (signal_pending(current))
		return -EIO;

	if (cpm_debug > 1) printk("back from sleep\n");

	if (tbdf->cbd_sc & BD_SC_NAK) {
		if (cpm_debug > 1) printk("IIC try; no ack\n");
		return 0;
	}
	
	if (tbdf->cbd_sc & BD_SC_READY) {
		printk("IIC try; complete but tbuf ready\n");
	}
	
	return 1;
}

static int cpm_xfer(struct i2c_adapter *i2c_adap,
		    struct i2c_msg msgs[],
		    int num)
{
	struct i2c_algo_8260_data *adap = i2c_adap->algo_data;
	struct i2c_msg *pmsg;
	int i, ret;
	u_char addr;

	for (i = 0; i < num; i++) {
		pmsg = &msgs[i];

		if (cpm_debug)
			printk("i2c-algo-8xx.o: "
			       "#%d addr=0x%x flags=0x%x len=%d\n",
			       i, pmsg->addr, pmsg->flags, pmsg->len);

		addr = pmsg->addr << 1;
		if (pmsg->flags & I2C_M_RD )
			addr |= 1;
		if (pmsg->flags & I2C_M_REV_DIR_ADDR )
			addr ^= 1;

		if (!(pmsg->flags & I2C_M_NOSTART)) {
		}
		if (pmsg->flags & I2C_M_RD ) {
			/* read bytes into buffer*/
			ret = cpm_iic_read(adap, addr, pmsg->buf, pmsg->len);
			if (cpm_debug)
				printk("i2c-algo-8260.o: read %d bytes\n", ret);
			if (ret < pmsg->len ) {
				return (ret<0)? ret : -EREMOTEIO;
			}
		} else {
			/* write bytes from buffer */
			ret = cpm_iic_write(adap, addr, pmsg->buf, pmsg->len);
			if (cpm_debug)
				printk("i2c-algo-8260.o: wrote %d\n", ret);
			if (ret < pmsg->len ) {
				return (ret<0) ? ret : -EREMOTEIO;
			}
		}
	}
	return (num);
}

static int algo_control(struct i2c_adapter *adapter,
	unsigned int cmd, unsigned long arg)
{
	return 0;
}

static u32 cpm_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_SMBUS_EMUL | I2C_FUNC_10BIT_ADDR |
	       I2C_FUNC_PROTOCOL_MANGLING;
}

/* -----exported algorithm data: -------------------------------------	*/

static struct i2c_algorithm cpm_algo = {
	"MPC8260 CPM algorithm",
	I2C_ALGO_MPC8260,
	cpm_xfer,
	NULL,
	NULL,				/* slave_xmit		*/
	NULL,				/* slave_recv		*/
	algo_control,			/* ioctl		*/
	cpm_func,			/* functionality	*/
};

/*
 * registering functions to load algorithms at runtime
 */
static
int i2c_8260_add_bus(struct i2c_adapter *adap)
{
	int i;
	struct i2c_algo_8260_data *cpm_adap = adap->algo_data;

	if (cpm_debug)
		printk("i2c-algo-8xx.o: hw routines for %s registered.\n",
		       adap->name);

	/* register new adapter to i2c module... */

	adap->id |= cpm_algo.id;
	adap->algo = &cpm_algo;

	i2c_add_adapter(adap);
	cpm_iic_init(cpm_adap);

	/* scan bus */
	if (cpm_scan) {
		printk(KERN_INFO " i2c-algo-8260.o: scanning bus %s...\n",
		       adap->name);
		for (i = 0; i < 128; i++) {
			if (cpm_iic_tryaddress(cpm_adap, i)) {
				printk("(%02x)",i<<1);
			}
		}
		printk("\n");
	}
	return 0;
}


int i2c_8260_del_bus(struct i2c_adapter *adap)
{
	int res;
	struct i2c_algo_8260_data *cpm_adap = adap->algo_data;

	cpm_iic_shutdown(cpm_adap);

	if ((res = i2c_del_adapter(adap)) < 0)
		return res;

	printk("i2c-algo-8260.o: adapter unregistered: %s\n",adap->name);

	return 0;
}

int __init i2c_algo_8260_init (void)
{
	printk("i2c-algo-8260.o: i2c mpc8xx algorithm module\n");
	return 0;
}

static void
m8260_iic_init(struct i2c_algo_8260_data *data)
{
	volatile cpm8260_t	*cp;
	volatile immap_t	*immap;
	volatile iop8260_t	*io;

	cp = cpmp;	/* Get pointer to Communication Processor */
	immap = (immap_t *)IMAP_ADDR;	/* and to internal registers */

	*(ushort *)(&immap->im_dprambase[PROFF_I2C_BASE]) = PROFF_I2C;
	data->iip = (iic_t *)&immap->im_dprambase[PROFF_I2C];

	data->i2c = (i2c8260_t *)&(immap->im_i2c);
	data->cp = cp;

	/* Initialize Port D IIC pins.
	*/
	io = &immap->im_ioport;
	io->iop_ppard |= 0x00030000;
	io->iop_pdird &= ~0x00030000;
	io->iop_psord |= 0x00030000;
	io->iop_podrd |= 0x00030000;

	/* Allocate space for two transmit and two receive buffer
	 * descriptors in the DP ram.
	 */
	data->dp_addr = m8260_cpm_dpalloc(sizeof(cbd_t) * 4, 4);

	/* ptr to i2c area */
	data->i2c = (i2c8260_t *)&(((immap_t *)IMAP_ADDR)->im_i2c);

	data->reloc = 0;
}

static int m8260_reg(struct i2c_client *client)
{
	return 0;
}

static int m8260_unreg(struct i2c_client *client)
{
	return 0;
}

static void m8260_inc_use(struct i2c_adapter *adap)
{
#ifdef MODULE
	MOD_INC_USE_COUNT;
#endif
}

static void m8260_dec_use(struct i2c_adapter *adap)
{
#ifdef MODULE
	MOD_DEC_USE_COUNT;
#endif
}

static struct i2c_algo_8260_data m8260_data;

static struct i2c_adapter m8260_ops = {
	"m8260",
	I2C_HW_MPC8260_EPON,
	NULL,
	&m8260_data,
	m8260_inc_use,
	m8260_dec_use,
	m8260_reg,
	m8260_unreg,
};

int __init i2c_m8260_init(void)
{
	printk("i2c-algo_8260.o: i2c MPC8260 module\n");

	/* reset hardware to sane state */
	m8260_iic_init(&m8260_data);

	if (i2c_8260_add_bus(&m8260_ops) < 0) {
		printk("i2c-algo-8260: Unable to register with I2C\n");
		return -ENODEV;
	}

	return 0;
}

void __exit i2c_m8260_exit(void)
{
	i2c_8260_del_bus(&m8260_ops);
}

#ifdef MODULE
MODULE_AUTHOR("Dan Malek <dmalek@jlc.net>");
MODULE_DESCRIPTION("I2C-Bus adapter routines for 8260");

module_init(i2c_m8260_init);
module_exit(i2c_m8260_exit);
#endif

