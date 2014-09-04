/* $Id: rtc-rx5c348.c,v 1.1.6.2 2002/11/08 11:30:28 takemura Exp $ */

/*
 *  RTC RICOH RxC5C348 routine 
 *
 *  Copyright (C) 2001 Sony Corporation. All rights reserved.
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
#include <linux/types.h>
#include <linux/time.h>
#include <linux/rtc.h>

#include <asm/au1000.h>
#include <asm/snsc_mpu200.h>

#define SECDAY		(60*60*24)
#define SECHOUR 	(60*60)
#define SECMINUTES	60
#define EPOC		1970
#define leapyear(year)	((year) % 4 == 0)
#define days_in_year(y)	(leapyear(y)? 366 : 356)
#define days_in_month(m)	(month_days[(m) - 1])

static int month_days[12] = {
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

void xdelay(int n)
{
	int i;

	for (i = 0; i < 20000 * n; i++)
		;
}

void rx5c348_rtc_init(void)
{
	static int inited = 0;

	volatile unsigned int *sci_reg = (unsigned int *)MPU200_MPLDSCI;
	if (!inited) {
		*sci_reg = MPU200_SCI_SCLK;
		xdelay(10);
		inited = 1;
	}
	return;
}


static unsigned char rx5c348_rtc_read(unsigned long addr)
{
	volatile unsigned int *sci_reg = (unsigned int *)MPU200_MPLDSCI;
	unsigned int sbits;
	unsigned int rbits;
	unsigned int stmp;
	int cnt;

	sbits = MPU200_RS5C348A_READ | 
		(addr << MPU200_RS5C348A_ADDRSHIFT);

	*sci_reg = MPU200_SCI_CE_RTC | MPU200_SCI_SCLK;	/* assert CS */
	au_sync();
	xdelay(10);

	for (cnt = 0; cnt < MPU200_RS5C348A_READ_SEND_BITS; cnt++) {
		stmp =
			(sbits & MPU200_RS5C348A_SEND_BIT_MASK) ?
				(MPU200_SCI_SO | MPU200_SCI_SI)	: 0;
		*sci_reg = MPU200_SCI_CE_RTC | stmp; /* SCLK = 0 */
		au_sync();
		xdelay(10);
		*sci_reg = MPU200_SCI_CE_RTC | MPU200_SCI_SCLK | stmp; /* SCLK=1 */
		au_sync();
		xdelay(10);
		sbits <<= 1;
	}

	rbits = 0;
	for (cnt = 0; cnt < MPU200_RS5C348A_READ_RECEIVE_BITS; cnt++) {
		rbits <<= 1;
		*sci_reg = MPU200_SCI_CE_RTC;	/* SCLK = 0 */
		au_sync();
		xdelay(10);
		rbits |= ((*sci_reg & MPU200_SCI_SI) ? 1 : 0);
		*sci_reg = MPU200_SCI_CE_RTC | MPU200_SCI_SCLK;	/* SCLK = 1 */
		au_sync();
		xdelay(10);
	}
	*sci_reg = MPU200_SCI_SCLK; /* negate CS */
		au_sync();
	return rbits;
}

static void rx5c348_rtc_write(unsigned char data, unsigned long addr)
{
	volatile unsigned int *sci_reg = (unsigned int *)MPU200_MPLDSCI;
	unsigned int sbits;
	unsigned int stmp;
	int cnt;

	sbits = MPU200_RS5C348A_WRITE |
		((addr << MPU200_RS5C348A_ADDRSHIFT) & MPU200_RS5C348A_ADDRMASK)
		| ((data << MPU200_RS5C348A_DATASHIFT) & MPU200_RS5C348A_DATAMASK);

	*sci_reg = MPU200_SCI_CE_RTC | MPU200_SCI_SCLK; /* assert CS */
	au_sync();
	xdelay(10);

	for (cnt = 0; cnt < MPU200_RS5C348A_WRITE_SEND_BITS; cnt++) {
		stmp =
			(sbits & MPU200_RS5C348A_SEND_BIT_MASK) ?
				(MPU200_SCI_SO | MPU200_SCI_SI)	: 0;
		*sci_reg = MPU200_SCI_CE_RTC | stmp; /* SCLK = 0 */
		au_sync();
		xdelay(10);
		*sci_reg = MPU200_SCI_CE_RTC | MPU200_SCI_SCLK | stmp; /* SCLK=1 */
		au_sync();
		xdelay(10);
		sbits <<= 1;
	}
	*sci_reg = MPU200_SCI_SCLK; /* negate CS */
	au_sync();
	xdelay(1000);
	return;	
}


static inline int bcd2hex(unsigned int bcd)
{
	unsigned int hi,low;

	hi = (bcd>>4)&0xf;
	low = bcd&0xf;

	if (hi > 10 || low > 10) {
		return -1;
	}
	return (hi * 10 + low);
}

struct rtc_time get_rtc_time_mmdd(void)
{
	unsigned int year, month, day;
	unsigned int hour, minutes, seconds;
	struct rtc_time tm;

	seconds = rx5c348_rtc_read(MPU200_RS5C348A_RTC_SECONDS);
	seconds = bcd2hex(seconds);

	minutes = rx5c348_rtc_read(MPU200_RS5C348A_RTC_MINUTES);
	minutes = bcd2hex(minutes);

	hour = rx5c348_rtc_read(MPU200_RS5C348A_RTC_HOURS);
	hour = bcd2hex(hour);

	day = rx5c348_rtc_read(MPU200_RS5C348A_RTC_DAY_OF_MONTH);
	day = bcd2hex(day);

	year = rx5c348_rtc_read(MPU200_RS5C348A_RTC_YEAR);
	year = bcd2hex(year);

	month = rx5c348_rtc_read(MPU200_RS5C348A_RTC_MONTH_100Y);
	if (month&0x80) {
		year += 100;
		month &= 0x7f;
	}
	month = bcd2hex(month);

	if (seconds >= 0 && minutes >= 0 && hour >= 0
		&& day >= 0 && year >= 0 && month >= 0) {
		tm.tm_year = year;
		tm.tm_mon = month;
		tm.tm_mday = day;
		tm.tm_hour = hour;
		tm.tm_min = minutes;
		tm.tm_sec = seconds;
	} else {
		tm.tm_year = 2001;
		tm.tm_mon = 11;
		tm.tm_mday = 1;
		tm.tm_hour = 0;
		tm.tm_min = 0;
		tm.tm_sec = 0;
	}
	return tm;
}

unsigned long get_rtc_time(void)
{
	unsigned int year, month, day;
	unsigned int hour, minutes, seconds;

	seconds = rx5c348_rtc_read(MPU200_RS5C348A_RTC_SECONDS);
	seconds = bcd2hex(seconds);

	minutes = rx5c348_rtc_read(MPU200_RS5C348A_RTC_MINUTES);
	minutes = bcd2hex(minutes);

	hour = rx5c348_rtc_read(MPU200_RS5C348A_RTC_HOURS);
	hour = bcd2hex(hour);

	day = rx5c348_rtc_read(MPU200_RS5C348A_RTC_DAY_OF_MONTH);
	day = bcd2hex(day);

	year = rx5c348_rtc_read(MPU200_RS5C348A_RTC_YEAR);
	year = bcd2hex(year);

	month = rx5c348_rtc_read(MPU200_RS5C348A_RTC_MONTH_100Y);
	if (month&0x80) {
		year += 2000;
		month &= 0x7f;
	} else {
		year += 1900;
	}
	month = bcd2hex(month);

	if (seconds >= 0 && minutes >= 0 && hour >= 0
		&& day >= 0 && year >= 0 && month >= 0) {
		return mktime(year, month, day, hour, minutes, seconds);
	} else {
		return mktime(2001, 1, 1, 0, 0, 0);
	}
}

static inline int hex2bcd(unsigned int bcd)
{
	unsigned int hi,low;

	hi = (bcd/10)%10;
	low = bcd%10;

	return ((hi << 4) + low);
}

int set_rtc_time_mmdd(int year, int month, int day, int hour, int minutes, int seconds)
{
	int ctrl;

	month = hex2bcd(month);
	if (year >= 2000) {
		month |= 0x80;
		year %= 100;
	} else {
		year %= 100;
	}
	year = hex2bcd(year);
	day = hex2bcd(day);

	hour = hex2bcd(hour);
	minutes = hex2bcd(minutes);
	seconds = hex2bcd(seconds);

	rx5c348_rtc_write(seconds, MPU200_RS5C348A_RTC_SECONDS);
	rx5c348_rtc_write(minutes, MPU200_RS5C348A_RTC_MINUTES);
	ctrl = rx5c348_rtc_read(MPU200_RS5C348A_RTC_CONTROL);
	if (!(ctrl&MPU200_RS5C348A_RTC_24H)) {
		/* make sure 24H hour mode */
		ctrl |= MPU200_RS5C348A_RTC_24H;
		rx5c348_rtc_write(ctrl, MPU200_RS5C348A_RTC_CONTROL);
	}
	rx5c348_rtc_write(hour, MPU200_RS5C348A_RTC_HOURS);
	rx5c348_rtc_write(day, MPU200_RS5C348A_RTC_DAY_OF_MONTH);
	rx5c348_rtc_write(month, MPU200_RS5C348A_RTC_MONTH_100Y);
	rx5c348_rtc_write(year, MPU200_RS5C348A_RTC_YEAR);

	return 0;
}

int set_rtc_time(unsigned long nowtime)
{
	int year, month, day, hour, seconds, minutes;
	int i;

	day = nowtime / SECDAY;
	seconds = nowtime % SECDAY;

	hour = seconds / SECHOUR;
	minutes = (seconds % SECHOUR) / SECMINUTES;
	seconds = (seconds % SECHOUR) % SECMINUTES;

	for (i = EPOC; day > days_in_year(i); i++);
		day -= days_in_year(i);

	year = i;

	if (leapyear(year))
		days_in_month(2) = 29;

	for (i = 1; day >= days_in_month(i); i++)
		day -= days_in_month(i);
	days_in_month(2) = 28;
	month = i;

	day = day + 1;

	return set_rtc_time_mmdd(year, month, day, hour, minutes, seconds);
}

void rtc_init(void)
{
	unsigned int ctrl;

	rx5c348_rtc_init();
	ctrl = rx5c348_rtc_read(MPU200_RS5C348A_RTC_CONTROL);
	if (!(ctrl&MPU200_RS5C348A_RTC_24H)) {
		/* 
		 * use 24H mode only,
		 * assume RTC was not initialized.
		 * set 2001/11/01 00:00
		 */
		ctrl |= MPU200_RS5C348A_RTC_24H;
		rx5c348_rtc_write(ctrl, MPU200_RS5C348A_RTC_CONTROL);
printk("rtc_init: invalid rtc data: set 2001/11/01 00:00\n");
		set_rtc_time_mmdd(2001,11,1,0,0,0);
	}
	xdelay(10000);
}
/* end */
