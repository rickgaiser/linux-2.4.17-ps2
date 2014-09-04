/*
 *  snsc_mpu300_siogpio.h : MPU-300 SuperIO GPIO driver API header file
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


#ifndef _SNSC_MPU300_SIOGPIO_H
#define _SNSC_MPU300_SIOGPIO_H

/* bit field */
#define SIOGPIO_BITS_ALL  0xffff

/* GPIO pin mode */
#define SIOGPIO_MODE_OUT            0x0001
#define SIOGPIO_MODE_IN             0x0002
#define SIOGPIO_MODE_NOINVERT       0x0004
#define SIOGPIO_MODE_INVERT         0x0008
#define SIOGPIO_MODE_PUSHPULL       0x0020
#define SIOGPIO_MODE_OPENDRAIN      0x0040

/* GPIO interrupt mode */
#define SIOGPIO_INTR_POSITIVE_EDGE  0
#define SIOGPIO_INTR_NEGATIVE_EDGE  1
#define SIOGPIO_INTR_BOTH_EDGE      2


#ifdef __KERNEL__
/* exported functions */

/* Initialize */
int   mpu300_siogpio_enable(__u16 bits, __u32 mode);

/* Data register access */
int   mpu300_siogpio_get_data_bit(int bit_num);
void  mpu300_siogpio_set_data_bit(int bit_num, int value);
__u16 mpu300_siogpio_get_data(__u16 bits);
void  mpu300_siogpio_set_data(__u16 bits, __u16 value);

/* Interrupt handling */
void  mpu300_siogpio_intr_enable( void );
void  mpu300_siogpio_intr_disable( void );
int   mpu300_siogpio_intr_register_handler(__u16 bits, __u32 mode,
					   void (*handler)(int irq, void *dev_id, struct pt_regs *regs),
					   void *dev_id);
void  mpu300_siogpio_intr_unregister_handler(__u16 bits);
void  mpu300_siogpio_intr_unmask_bit(int bit_num);
void  mpu300_siogpio_intr_unmask(__u16 bits);
void  mpu300_siogpio_intr_mask_bit(int bit_num);
void  mpu300_siogpio_intr_mask(__u16 bits);
int   mpu300_siogpio_intr_get_status_bit(int bit_num);
__u16 mpu300_siogpio_intr_get_status(__u16 bits);
void  mpu300_siogpio_intr_clear_bit(int bit_num);
void  mpu300_siogpio_intr_clear( void );

#endif /* __KERNEL__ */

#endif /* _SNSC_MPU300_SIOGPIO_H */
