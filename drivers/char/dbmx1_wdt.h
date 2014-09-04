/*
 *  dbmx1_wdt.h -- Dragonball MX1 Watch Dog Timer driver
 *
 */
/*
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

#ifndef DBMX1_WDT_H_
#define DBMX1_WDT_H_

#define WDT_IO_BASE     0x00201000            /* Watchdog Timer Base Addr */
#define IO_LENGTH       4*3                   /* Watchdog Timer Reg Length */

#define WDT_WCR         WDT_IO_BASE           /* Watchdog Control Reg Addr */
#define WDT_WSR         WDT_IO_BASE+4         /* Watchdog Service Reg Addr */
#define WDT_WSTR        WDT_IO_BASE+8         /* Watchdog Statue  Reg Addr */

#define WDT_IRQ         63                    /* Watchdog IRQ Number */

#define WDT_DEV_NAME    "dbmx1_wdt"	      /* Device Name */

#define WDT_TIMEOUT     (1*2*60)<<8   	      /* Watchdog Timeout value */
                                              /* 60sec (1=0.5sec)       */

#define WIE_ON          0x00000010            /* Watchdog interrupt Enable  */
#define WIE_OFF         0x00000000            /*                    Disable */

#define TMD_ON          0x00000008            /* Test Mode Counter Clock 2Hz*/
#define TMD_OFF         0x00000000            /*                         32K*/

#define SWR_ON          0x00000004            /* Software Reset Enable  */
#define SWR_OFF         0x00000000            /*                Disable */

#define WDEC_ON         0x00000002            /* Watchdog Enable Control ON */
#define WDEC_OFF        0x00000000            /*                         OFF*/

#define WDE_ON          0x00000001            /* Watchdog Enable  */
#define WDE_OFF         0x00000000            /*          Disable */

#define WDS_DATA1       0x00005555            /* Watchdog Service data 1 */
#define WDS_DATA2       0x0000AAAA            /*                       2 */

#define WSTR_TINT       0x00000100            /* Time-Out interrupt mask */
#define WSTR_TOUT       0x00000001            /* Time-Out mask */

#endif /* !DBMX1_WDT_H_ */
