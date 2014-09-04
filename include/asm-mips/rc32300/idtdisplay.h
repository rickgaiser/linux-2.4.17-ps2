/*
 *
 * BRIEF MODULE DESCRIPTION
 *	79S334A 4 digits display.
 *
 * Copyright 2002 THOMSON multimedia.
 * Author: Stephane Fillod & Guillaume Lorand
 *         	fillods@thmulti.com
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
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

#ifndef _IDTDISPLAY_H
#define _IDTDISPLAY_H

#define IDTDISPLAY_BUF_SIZE 256 /**< size of the internal buffer */

#define IDTDISPLAY_IOC_MAGICNUM 'x'
#define IDTDISPLAY_IOC_BASE 0x00


/**
 * clean the display
 */
#define IDTDISPLAY_IOCTL_CLEAN         _IO   (IDTDISPLAY_IOC_MAGICNUM, (IDTDISPLAY_IOC_BASE +  0))

/**
 * write one char on the display
 * see idtdisplay_wc_struct
 */
#define IDTDISPLAY_IOCTL_WRITE_CHAR    _IOW  (IDTDISPLAY_IOC_MAGICNUM, (IDTDISPLAY_IOC_BASE +  1), struct idtdisp_wc_struct)

/**
 * write four chars on the display
 */
#define IDTDISPLAY_IOCTL_WRITE_4       _IOW  (IDTDISPLAY_IOC_MAGICNUM, (IDTDISPLAY_IOC_BASE +  2), u_long)

/**
 * set up an new delay between scrolling
 */
#define IDTDISPLAY_IOCTL_DELAY         _IOW  (IDTDISPLAY_IOC_MAGICNUM, (IDTDISPLAY_IOC_BASE +  3), u_int)


/**
 * structure passed to ioctl function for IDTDISPLAY_IOCTL_WRITE_CHAR
 */
struct idtdisp_wc_struct {
  char ch ; /**< character to display */
  int nb ;  /**< number of the display */
};

#ifdef __KERNEL__

/** @name __KERNEL__
*/
//@{
#define IDTDISPLAY_MINOR 254 /**< default minor number */
#define IDTDISPLAY_DELAY 500 /**< default inter scrolling delay in ms */


/* 100 = 1 seconde */
/* 100/1000 = 1 milli-second = 0,1 */
#define MS_TO_HZ(ms) ((ms) * HZ / 1000) /** convert delay in millisecond to jiffies */

#endif // __KERNEL__
//@}


#endif // _IDTDISPLAY_H
