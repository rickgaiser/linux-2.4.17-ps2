/******************************************************************************
 * 
 *	File:	linux/drivers/char/snsc_mpu200.c
 *
 *	Purpose:	Support for  architecture keypad.
 *
 *	Copyright 2001 Sony Corporation.
 *
 *	This program is free software; you can redistribute  it and/or modify it
 *	under  the terms of  the GNU General  Public License as published by the
 *	Free Software Foundation;  either version 2 of the	License, or (at your
 *	option) any later version.
 *
 *	THIS  SOFTWARE	IS PROVIDED   ``AS	IS'' AND   ANY	EXPRESS OR IMPLIED
 *	WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *	MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *	NO	EVENT  SHALL   THE AUTHOR  BE	 LIABLE FOR ANY   DIRECT, INDIRECT,
 *	INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *	NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS	OR SERVICES; LOSS OF
 *	USE, DATA,	OR PROFITS; OR	BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *	ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *	(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *	THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	You should have received a copy of the	GNU General Public License along
 *	with this program; if not, write  to the Free Software Foundation, Inc.,
 *	675 Mass Ave, Cambridge, MA 02139, USA.
 *****************************************************************************/
//#define DEBUG_KPAD
#define SOFT_RMV_CHAT
//#define HARD_RMV_CHAT

#include <linux/module.h>
#include <linux/list.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/config.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/kbd_ll.h>
#include <asm/io.h>
#include <asm/keyboard.h>
#include <asm/uaccess.h>
#include <asm/au1000.h>
#include <asm/snsc_mpu200.h>

#define INTVL_TIMER 	3
#ifdef HARD_RMV_CHAT
#define INTVL_ROW_DELAY		100
#elif defined( SOFT_RMV_CHAT )
#define INTVL_ROW_DELAY 	10
#endif

#define INTVL_CHAT_DELAY 	50	

typedef struct _SCANINFO {
	char	key[8],
		lastkey[8],
		diffkey[8],
		push_flg,
		settime;
} SCANINFO;

void __init kpad_hw_init( void);
int kpad_translate( unsigned char, unsigned char *);
void kpad_scan( SCANINFO *);

static struct timer_list kpad_timer = {
	function:	NULL
};

static SCANINFO scaninfo; 
/*---------------------------------------------------------------------------*/
/*    kpad_translate                                                         */
/*---------------------------------------------------------------------------*/
int kpad_translate( unsigned char scancode, unsigned char *keycode)
{
	// Translate scan code stream to key code
	// 
	// Our scan coding is simple: each key up/down event generates
	// a single scan code.
	//
	// TBD We translate scancode to keycode regardless of up/down status

	*keycode = scancode & ~KBUP;	// Mask off KBUP flag
	return 1;			// keycode is valid
}

/*---------------------------------------------------------------------------*/
/*    kbd_irq_handler                                                        */
/*---------------------------------------------------------------------------*/
void kbd_irq_handler( int irq, void *dev_id, struct pt_regs *regs)
{
	SCANINFO	*sinfo = (SCANINFO *)dev_id;

	/* KeyPad Interrupt Disable */
	outl( inl(MPU200_MPLDMAC) & ~MPU200_KPINTR_EN, MPU200_MPLDMAC ); 

	sinfo->push_flg = 0;

       	kpad_scan( sinfo );

        return;
}
 
/*---------------------------------------------------------------------------*/
/*    kpad_scan                                                              */
/*---------------------------------------------------------------------------*/
void kpad_scan( SCANINFO *sinfo )
{
	unsigned char	row_line,
			cal_line,
			downkey, upkey;
	unsigned int 	i, j;


	udelay(INTVL_CHAT_DELAY);

	if( sinfo ==  NULL) { 
		printk("snsc_mpu200_kpad: sinfo is null\n");
		return;
    	}

	for (i = 0, row_line = 0x01; i < 8; i++, row_line <<= 1) {

		outl( ~row_line, MPU200_KEYPADROW );
		udelay(INTVL_ROW_DELAY);
		sinfo->key[i] = inl(MPU200_KEYPADCOL);

		if(~(sinfo->key[i])){
			sinfo->push_flg = 1;
		}

		sinfo->diffkey[i] = sinfo->key[i] ^ sinfo->lastkey[i];

		if( sinfo->diffkey[i] ){
			downkey = ~sinfo->key[i] & sinfo->diffkey[i];	/* key went down */
			upkey	= sinfo->key[i]  & sinfo->diffkey[i];	/* key went up 	 */

			for( j = 0, cal_line = 0x80; j < 8; j++, cal_line >>= 1){

				if( downkey & cal_line )
					handle_scancode( KEYCODE( i, j ), 1 );
				else if ( upkey & cal_line )
					handle_scancode( KEYCODE( i, j ) | KBUP, 0 );
			}
		}
		sinfo->lastkey[i]  = sinfo->key[i];
		udelay(INTVL_ROW_DELAY);
	}

        if( sinfo->push_flg ){

                /* Timer Set */
                kpad_timer.expires = jiffies+sinfo->settime;
                kpad_timer.data = ( unsigned long )sinfo;
                kpad_timer.function = kpad_scan;
                if(!(timer_pending(&kpad_timer))){
                        add_timer(&kpad_timer);
                }
		sinfo->push_flg = 0;

        } else {
		outl( 0xff, MPU200_KEYPADROW );
		udelay(INTVL_ROW_DELAY);
		outl( 0x00, MPU200_KEYPADROW );

		/* KeyPad Interrput Enable */
		outl( inl(MPU200_MPLDMAC) | MPU200_KPINTR_EN, MPU200_MPLDMAC );
	}
	return;
}

/*---------------------------------------------------------------------------*/
/*    kpad_hw_init                                                           */
/*---------------------------------------------------------------------------*/
void __init kpad_hw_init(void)
{
	SCANINFO	*sinfo = &scaninfo;
	int i;

        for (i = 0; i < 8; i++) {
                sinfo->key[i]      = 0xFF;
                sinfo->lastkey[i]  = 0xFF;
                sinfo->diffkey[i]  = 0x00;
        }
        sinfo->push_flg = 0;
	sinfo->settime  = INTVL_TIMER;

	init_timer(&kpad_timer);

	outl( inl(MPU200_MPLDMAC) & ~MPU200_KPINTR_EN, MPU200_MPLDMAC ); 

	outl( 0xff, MPU200_KEYPADROW );
	udelay(INTVL_ROW_DELAY);
	outl( 0x00, MPU200_KEYPADROW );

	if (request_irq( AU1000_KEYPAD_INT, kbd_irq_handler, SA_INTERRUPT,
			 "snsc_mpu200_kpad", sinfo ))
	{
		printk( KERN_ERR "snsc mpu200 keypad driver aborting"
				 " due to AU1000_KEYPAD_INT unavailable.\n");
        }

	outl( inl(MPU200_MPLDMAC) | MPU200_KPINTR_EN, MPU200_MPLDMAC); 
}
