/*
 *  RTC Motorola MC9328MX1 routine (header)
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


#ifndef SNSC_MPU110_RTC_H_
#define SNSC_MPU110_RTC_H_

#include <linux/types.h>

#define MPU110_RTC_RTCIENR_ALM  2
#define MPU110_RTC_RTCIENR_1HZ  4

static void mpu110_rtc_init(void);
static void mpu110_rtc_release(void);
static void mpu110_mask_rtc_irq_bit(unsigned char bit);
static void mpu110_set_rtc_irq_bit(unsigned char bit);
static unsigned char mpu110_reset_interrupt(void);
static int mpu110_get_rtc_time(struct rtc_time *rtc_tm);
static void mpu110_set_rtc_time(int days, int hrs, int min, int sec);
static void mpu110_rtc_set_alarm(int days, int hour, int min, int sec);
static void mpu110_get_rtc_alm_time(struct rtc_time *alm_tm);
static u32 mpu110_rtc_read_RTCIENR(void);
#ifdef DEBUG
static u32 mpu110_rtc_read_RTCISR(void);
static u32 mpu110_rtc_read_RCCTL(void);
#endif

#endif /* SNSC_MPU110_RTC_H_ */
