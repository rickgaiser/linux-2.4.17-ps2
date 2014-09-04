/*
 *  snsc_mpu301_kpad.h : MPU-301 SuperIO GPIO keypad driver header file
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

#ifndef _MPU301_KPAD_H
#define _MPU301_KPAD_H

#define MPU301_KPAD_MINOR -1;
#define MPU301_KPAD_NAME "mpu301_kpad"

#define MPU301_KPAD_SCAN_DELAY 10   /* usec */

#define MPU301_KPAD_ROWS 8
#define MPU301_KPAD_COLS 8


static int mpu301_kpad_init( void );
static void mpu301_kpad_final( void );
static int mpu301_kpad_open( struct keypad_dev * );
static int mpu301_kpad_release( struct keypad_dev * );
static void mpu301_kpad_intr_handler(int, void *, struct pt_regs *);
static void mpu301_kpad_tasklet(unsigned long);
static void mpu301_kpad_key_scan(unsigned long);
#ifdef MODULE
static int mpu301_kpad_init_module( void );
static void mpu301_kpad_cleanup_module( void );
#endif /* MODULE */


#endif /* _MPU301_KPAD_H */
