/*
 *  linux/include/linux/snsc_bu9929_gpio.h
 * 
 *  GPIO functions for ROHM BU9929FV on I2C(SMBus)
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

#ifndef __SNSC_BU9929_GPIO_H__
#define __SNSC_BU9929_GPIO_H__

#include <linux/ioctl.h>

struct bu9929_iocdata {
        __u8   id;
        __u16  val;
};

#define BU9929_IOC_MAGIC  'b'
#define BU9929_IOC_SET_INTRMODE       _IOW(BU9929_IOC_MAGIC, 0, struct bu9929_iocdata)
#define BU9929_IOC_SHUTDOWN_ON        _IOW(BU9929_IOC_MAGIC, 1, struct bu9929_iocdata)
#define BU9929_IOC_SHUTDOWN_OFF       _IOW(BU9929_IOC_MAGIC, 2, struct bu9929_iocdata)
#define BU9929_IOC_DIR                _IOW(BU9929_IOC_MAGIC, 3, struct bu9929_iocdata)
#define BU9929_IOC_OUT                _IOW(BU9929_IOC_MAGIC, 4, struct bu9929_iocdata)
#define BU9929_IOC_IN                 _IOWR(BU9929_IOC_MAGIC, 5, struct bu9929_iocdata)

/* for set intrmode */
#define BU9929_INT_PULSE      (1 << 6)
#define BU9929_INT_LEVEL      (0 << 6)
#define BU9929_WATCH_0_15     (1 << 2)
#define BU9929_WATCH_0_7      (0 << 2)
#define BU9929_TRIG_DOUBLE    (1 << 1)
#define BU9929_TRIG_NEGATIVE  (0 << 1)
#define BU9929_WATCH_ENABLE   (1 << 0)
#define BU9929_WATCH_DISABLE  (0 << 0)
#define BU9929_INTMODE_MASK   (0x47)

#ifdef __KERNEL__
extern int bu9929gpio_set_intrmode(int id, int mode);
extern int bu9929gpio_shutdown_on(int id);
extern int bu9929gpio_shutdown_off(int id);
extern int bu9929gpio_dir(int id, __u16 dir);
extern int bu9929gpio_out(int id, __u16 value);
extern int bu9929gpio_in(int id, __u16 *value);
#endif /* __KERNEL__ */

#endif /* __SNSC_BU9929_GPIO_H__ */
