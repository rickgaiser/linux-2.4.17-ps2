/*
 *  linux/include/linux/snsc_ich_gpio.h
 * 
 *  GPIO functions for ICH2
 *
 *  Copyright 2001,2002 Sony Corporation.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License.
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

#ifndef __SNSC_ICH_GPIO_H__
#define __SNSC_ICH_GPIO_H__

#include <linux/ioctl.h>

struct ichgpio_iocdata {
        __u32  id;
        __u32  val;
};

#define ICHGPIO_IOC_MAGIC  'i'
#define ICHGPIO_IOC_USESEL       _IOW(ICHGPIO_IOC_MAGIC, 0, struct ichgpio_iocdata)
#define ICHGPIO_IOC_IOSEL        _IOW(ICHGPIO_IOC_MAGIC, 1, struct ichgpio_iocdata)
#define ICHGPIO_IOC_SIGINV       _IOW(ICHGPIO_IOC_MAGIC, 2, struct ichgpio_iocdata)
#define ICHGPIO_IOC_CLRSTAT      _IO(ICHGPIO_IOC_MAGIC, 3)
#define ICHGPIO_IOC_READSTAT     _IO(ICHGPIO_IOC_MAGIC, 4)
#define ICHGPIO_IOC_IN           _IO(ICHGPIO_IOC_MAGIC, 5)
#define ICHGPIO_IOC_OUT          _IO(ICHGPIO_IOC_MAGIC, 6)


/* for ichgpio_use_sel */
#define ICHGPIO_USESEL_NATIVE   0
#define ICHGPIO_USESEL_GPIO     1

/* for ichgpio_io_sel */
#define ICHGPIO_IOSEL_OUT       0
#define ICHGPIO_IOSEL_IN        1

/* for ichgpio_sig_inv */
#define ICHGPIO_SIGINV_HIGH     0
#define ICHGPIO_SIGINV_LOW      1

/* for ichgpio_ctrl_rout */
#define ICHGPIO_CTRLROUT_NO     0
#define ICHGPIO_CTRLROUT_SMI    1
#define ICHGPIO_CTRLROUT_SCI    2

#ifdef __KERNEL__
extern void ichgpio_use_sel(int id, __u32 sel);
extern void ichgpio_io_sel(int id, __u32 sel);
extern void ichgpio_sig_inv(int id, __u32 level);
extern void ichgpio_ctrl_rout(int id, __u32 rout);
extern void ichgpio_enable_intr(int id);
extern void ichgpio_disable_intr(int id);
extern void ichgpio_clear_intr(int id);
extern int ichgpio_int_status(int id);
extern __u32 ichgpio_in(void);
extern void ichgpio_out(__u32 v);
extern int ichgpio_irq(void);
extern int ichgpio_init(void);
#endif /* __KERNEL__ */

#endif /* __SNSC_ICH_GPIO_H__ */
