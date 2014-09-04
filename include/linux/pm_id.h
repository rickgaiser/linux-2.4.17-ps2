/*
 *  PowerManagement ID information
 *
 *  Copyright 2002 Sony Corporation.
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

#ifndef __PM_ID_H__
#define __PM_ID_H__

/*
 *  MPU-110 series (DragonBall MX1) 
 *
 *    Note: The range 0x78000000-0x78ffffff of device ID is reserved
 *          for Company's use.
 */

/* USB Device */
#define DBMX1_USBD_PM_ID          0x75736264  /* "usbd" */
#define DBMX1_USBD_PM_NAME        "usbd"
#define DBMX1_USBD_PM_ID_STATE_0           0  /* normal */
#define DBMX1_USBD_PM_ID_STATE_2           2  /* stop: Wakeup at interrupt only by insert */
#define DBMX1_USBD_PM_ID_STATE_3           3  /* stop: Power off */

/* UART */
/* 
   see pm.h
 */

/* CPU and Memory.  Call it last. */
#define DBMX1_CPU_PM_ID           0x636f7265  /* "core" */
#define DBMX1_CPU_PM_NAME         "core"
#define DBMX1_CPU_PM_ID_STATE_1            1  /* CPU off, Memory 1Mhz */
#define DBMX1_CPU_PM_ID_STATE_2            2  /* CPU off, Memory off, Wait for interrupt */

/* MPU-110 LCD */
#define DBMX1_MPU110FB_PM_ID      0x66623131
#define DBMX1_MPU110FB_PM_NAME    "MPU110FB"

/*
-------------+------------+------------------------------------------------
   Device    |   Dev.ID   |                    State
-------------+------------+------------------------------------------------
USBD         | 0x75736264 | D0: normal :
(Type: 0x0)  |            | D1: -      :
             |            | D2: stop   : Wakeup at interrupt only by insert
             |            | D3: stop   : Power Off
-------------+------------+------------------------------------------------
UART1        | 0x41d00500 | D0: normal :
UART2        |            | D1: stop   : Wakeup at interrupt
(Type: 0x1)  |            | D2: -      :
             |            | D3: stop   : Power Off
-------------+------------+------------------------------------------------
CPU/SDRAM    | 0x636f7265 | D0: normal : CPU=Max, Memory=Max
(Type: 0x1)  |            | D1: slow   : CPU=0Mhz, Memory=1Mhz
             |            | D2: wait   : CPU,Mem stop. Wait for Interrupt
             |            | D3: stop   : Shutdown
-------------+------------+------------------------------------------------
MPU-110 LCD  | 0x66623131 | D0: normal : Front light On
             | (Type:0x0) | D1: dark   : Front light Off
-------------+------------+------------------------------------------------
*/

#endif /* __PM_ID_H__ */
