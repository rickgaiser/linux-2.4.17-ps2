/*
 * Stowaway (TM) Portable Keyboard for the Compaq iPAQ 
 * Line discipline for Linux
 * 
 * Copyright 2001 Compaq Computer Corporation.
 * Stowaway is a trademark of Think Outside, Inc. (http://www.thinkoutside.com)
 *
 * Use consistent with the GNU GPL is permitted,
 * provided that this copyright notice is
 * preserved in its entirety in all copies and derived works.
 *
 * COMPAQ COMPUTER CORPORATION MAKES NO WARRANTIES, EXPRESSED OR IMPLIED,
 * AS TO THE USEFULNESS OR CORRECTNESS OF THIS CODE OR ITS
 * FITNESS FOR ANY PARTICULAR PURPOSE.
 *
 * Author: Andrew Christian 
 *         <andyc@handhelds.org>
 *         5 April 2001
 * 
 * Based in part on the Stowaway PDA keyboard driver for Linux
 * by Justin Cormack <j.cormack@doc.ic.ac.uk>
 *
 *
 *  General notes:
 *
 *  The "Fn" Function key is trapped internally and switches between keycode tables.  It acts as
 *  a virtual "shift" key for keycodes.  The skbd_keycode table shows keycode mappings for 
 *  the stowaway's keys in normal, function, and numlock mode.
 *
 *  The NUMLOCK state is based on the callback from keyboard.c: we use this internally
 *  to handle switching between the numeric keypad functions and the regular keys.
 *
 *
 *  Known issues/todos:
 *
 *   1. Holding down a key and unplugging the keyboard will cause an endless repeat until
 *      you re-insert the keyboard and press another key.  We should be able to cancel the
 *      repeat as soon as the keyboard is re-inserted.
 * 
 *   2. Allow ioctl calls to change the keycode mapping tables.
 *  
 *   3. Provide for a "probing" functionality so the user-space program could check to see
 *      if a keyboard is actually present.
 *   
 *   4. Fix power management during suspend/resume.  Right now we depend on the kernel
 *      serial driver to turn on and off the RS232 power during suspend/resume cycles.
 *
 *
 *  Targus Stowaway Logic
 *
 *      The keyboard receives power from the DTR line on the H3600 cable.  The DTR line is 
 *      held high as long as the EGPIO_H3600_RS232_ON extended GPIO line has power.  If we ever
 *      lower this line (say, in a low power mode), the keyboard will be unable to wake the 
 *      unit.
 *
 *      RTS controls the state of the keyboard: low = sleeping, high = awake.  The keyboard
 *      needs a transition on RTS to change states.
 *   
 *      The keyboard sends a 100 millisecond low-high-low transition on DCD under the following
 *      conditions:  (a) it is first powered up or plugged in, (b) it is asleep and someone
 *      presses a key on the keyboard.
 *
 *      Upon receiving a low-to-high transition on RTS, the first two characters sent by the 
 *      keyboard are 0xFA, 0xFD.  Subsequently, the keyboard sends a single character for each
 *      keypress and keyrelease.  If no keys are depressed, the final keyrelease is repeated
 *      once.  Characters are sent at 9600 8N1.
 *
 */


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/devfs_fs_kernel.h>
#include <linux/kbd_ll.h>
#include <linux/init.h>
#include <linux/kbd_kern.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <linux/sysctl.h>

#include <asm/bitops.h>
#include <asm/irq.h>
#include <asm/hardware.h>

MODULE_AUTHOR("Andrew Christian <andyc@handhelds.org>");

/* These keycodes are the defaults used by the pc keyboard */
#define SKEY_ESC		1		
#define SKEY_1			2		
#define SKEY_2			3		
#define SKEY_3			4		
#define SKEY_4			5		
#define SKEY_5			6		
#define SKEY_6			7		
#define SKEY_7			8		
#define SKEY_8			9		
#define SKEY_9			10		
#define SKEY_0			11		
#define SKEY_MINUS		12		
#define SKEY_EQUAL		13		
#define SKEY_BACKSPACE		14		/* Technically delete */
#define SKEY_TAB		15		
#define SKEY_Q			16		
#define SKEY_W			17		
#define SKEY_E			18		
#define SKEY_R			19		
#define SKEY_T			20		
#define SKEY_Y			21		
#define SKEY_U			22		
#define SKEY_I			23		
#define SKEY_O			24		
#define SKEY_P			25		
#define SKEY_LEFTBRACE		26		/* Bracket-left */
#define SKEY_RIGHTBRACE		27		/* Bracket-right */
#define SKEY_ENTER		28		/* Return */
#define SKEY_LEFTCTRL		29		/* Control */
#define SKEY_A			30		
#define SKEY_S			31		
#define SKEY_D			32		
#define SKEY_F			33		
#define SKEY_G			34		
#define SKEY_H			35		
#define SKEY_J			36		
#define SKEY_K			37		
#define SKEY_L			38		
#define SKEY_SEMICOLON		39		
#define SKEY_APOSTROPHE		40		
#define SKEY_GRAVE		41		
#define SKEY_LEFTSHIFT		42		/* Shift */
#define SKEY_BACKSLASH		43		
#define SKEY_Z			44		
#define SKEY_X			45		
#define SKEY_C			46		
#define SKEY_V			47		
#define SKEY_B			48		
#define SKEY_N			49		
#define SKEY_M			50		
#define SKEY_COMMA		51		
#define SKEY_DOT		52		/* period */
#define SKEY_SLASH		53		
#define SKEY_RIGHTSHIFT		54		/* Shift */
#define SKEY_KPASTERISK		55		/* KP_Multiply */
#define SKEY_LEFTALT		56		/* Alt */
#define SKEY_SPACE		57		
#define SKEY_CAPSLOCK		58		
#define SKEY_F1			59		
#define SKEY_F2			60		
#define SKEY_F3			61		
#define SKEY_F4			62		
#define SKEY_F5			63		
#define SKEY_F6			64		
#define SKEY_F7			65		
#define SKEY_F8			66		
#define SKEY_F9			67		
#define SKEY_F10		68		
#define SKEY_NUMLOCK		69		
#define SKEY_SCROLLLOCK		70		
#define SKEY_KP7		71		
#define SKEY_KP8		72		
#define SKEY_KP9		73		
#define SKEY_KPMINUS		74		
#define SKEY_KP4		75		
#define SKEY_KP5		76		
#define SKEY_KP6		77		
#define SKEY_KPPLUS		78		/* KP_Add */
#define SKEY_KP1		79		
#define SKEY_KP2		80		
#define SKEY_KP3		81		
#define SKEY_KP0		82		
#define SKEY_KPDOT		83		/* KP_Period */
#define SKEY_LASTCONSOLE	84      /* Last Console -- 103rd */
#define SKEY_UNASSIGNED_1	85
#define SKEY_102ND		86	/* < > */	
#define SKEY_F11		87		
#define SKEY_F12		88		
#define SKEY_UNASSIGNED_2	89
#define SKEY_UNASSIGNED_3	90	/* -- unassigned -- */			
#define SKEY_UNASSIGNED_4	91	/* -- unassigned -- */			
#define SKEY_UNASSIGNED_5	92	/* -- unassigned -- */			
#define SKEY_UNASSIGNED_6	93	/* -- unassigned -- */			
#define SKEY_UNASSIGNED_7	94	/* -- unassigned -- */			
#define SKEY_UNASSIGNED_8	95	/* -- unassigned -- */			
#define SKEY_KPENTER		96		
#define SKEY_RIGHTCTRL		97		/* Control */
#define SKEY_KPSLASH		98		/* KP_Divide */
#define SKEY_SYSRQ		99	/* Control_backslash */
#define SKEY_RIGHTALT		100		/* AltGr */
#define SKEY_LINEFEED		101     /* Break */
#define SKEY_HOME		102	/* Find  */
#define SKEY_UP			103	
#define SKEY_PAGEUP		104	/* Prior */	
#define SKEY_LEFT		105		
#define SKEY_RIGHT		106		
#define SKEY_END		107	/* Select */
#define SKEY_DOWN		108	
#define SKEY_PAGEDOWN		109	/* Next */	
#define SKEY_INSERT		110		
#define SKEY_DELETE		111             /* Remove */
#define SKEY_MACRO		112		
#define SKEY_MUTE		113	/* F13 */
#define SKEY_VOLUMEDOWN		114	/* F14 */
#define SKEY_VOLUMEUP		115	/* Help */	
#define SKEY_POWER		116	/* Do   */	
#define SKEY_KPEQUAL		117	/* F17  */
#define SKEY_KPPLUSMINUS	118     /* KP_MinPlus */
#define SKEY_PAUSE		119		
#define SKEY_F21		120    	/* -- unassigned -- */	
#define SKEY_F22		121	/* -- unassigned -- */	
#define SKEY_F23		122	/* -- unassigned -- */			
#define SKEY_F24		123	/* -- unassigned -- */			
#define SKEY_KPCOMMA		124	/* -- unassigned -- */	
#define SKEY_LEFTMETA		125	/* -- unassigned -- */			
#define SKEY_RIGHTMETA		126	/* -- unassigned -- */			
#define SKEY_COMPOSE		127	/* -- unassigned -- */			

#define SKEY_FUNCTION           128     /* The function key does not generate a scancode */

/*      ---------------- Keycode --------------                 --- Text on keyboard --- */
/*      Normal          Function        NumLock              #  White     Blue    Yellow */
static unsigned char skbd_keycode[] = {
	SKEY_1,         SKEY_F1,        0,               /*  0  1 !                      */
	SKEY_2,         SKEY_F2,        0,	         /*  1  2 @                      */
	SKEY_3,         SKEY_F3,        0,	         /*  2  3 #                      */
	SKEY_Z,         0,              0,	         /*  3  z                        */
	SKEY_4,         SKEY_F4,        0,	         /*  4  4 $                      */
	SKEY_5,         SKEY_F5,        0,	         /*  5  5 %                      */
	SKEY_6,         SKEY_F6,        0,	         /*  6  6 ^                      */
	SKEY_7,         SKEY_F7,        0,               /*  7  7 &                7     */
	SKEY_FUNCTION,  0,              0,	         /*  8            Fn             */
	SKEY_Q,         0,              0,	         /*  9  q                        */
	SKEY_W,         0,              0,	         /* 10  w                        */
	SKEY_E,         0,              0,	         /* 11  e                        */
	SKEY_R,         0,              0,	         /* 12  r                        */
	SKEY_T,         0,              0,	         /* 13  t                        */
	SKEY_Y,         0,              0,	         /* 14  y                        */
	SKEY_GRAVE,     0,              0,               /* 15  ` ~                      */
	SKEY_X,         0,              0,	         /* 16  x                        */
	SKEY_A,         0,              0,	         /* 17  a                        */
	SKEY_S,         0,              0,	         /* 18  s                        */
	SKEY_D,         0,              0,	         /* 19  d                        */
	SKEY_F,         0,              0,	         /* 20  f                        */
	SKEY_G,         0,              0,	         /* 21  g                        */
	SKEY_H,         0,              0,	         /* 22  h                        */
	SKEY_SPACE,     0,              0,	         /* 23  spacebar                 */  
	SKEY_CAPSLOCK,  SKEY_NUMLOCK,   0,	         /* 24  CapsLock  NumLock        */
	SKEY_TAB,       0,              0,	         /* 25  Tab                      */
	SKEY_LEFTCTRL,  0,              0,               /* 26  Ctrl(L)                  */
	0, 0, 0,	/* 27 */
	0, 0, 0,	/* 28 */
	0, 0, 0,	/* 29 */
	0, 0, 0,	/* 30 */
	0, 0, 0,        /* 31 */
	0, 0, 0,	/* 32 */
	0, 0, 0,	/* 33 */
	0,               0,             0,	         /* 34  windows_icon             */
	SKEY_LEFTALT,    0,             0,	         /* 35  Alt                      */
	0, 0, 0,	/* 36 */
	0, 0, 0,	/* 37 */
	0, 0, 0,	/* 38 */
	0, 0, 0,        /* 39 */
	0, 0, 0,	/* 40 */
	0, 0, 0,	/* 41 */
	0, 0, 0,	/* 42 */
	0, 0, 0,	/* 43 */
	SKEY_C,          0,             0,	         /* 44  c                        */
	SKEY_V,          0,             0,	         /* 45  v                        */
	SKEY_B,          0,             0,	         /* 46  b                        */
	SKEY_N,          0,             0,               /* 47  n                        */
	SKEY_MINUS,      0,             0,	         /* 48  - _                      */
	SKEY_EQUAL,      0,             0,	         /* 49  = +                      */
	SKEY_BACKSPACE,  0,             0,	         /* 50 Backspace  Off            */
	SKEY_HOME,       0,             0,	         /* 51 Inbox      Notes          */
	SKEY_8,          SKEY_F8,       SKEY_KP8,	 /* 52  8 *                8     */
	SKEY_9,          SKEY_F9,       SKEY_KP9,	 /* 53  9 (                9     */
	SKEY_0,          SKEY_F10,      SKEY_KPSLASH,	 /* 54  0 )                /     */
	SKEY_ESC,        0,             0,               /* 55  Space (not spacebar!)    */
        SKEY_LEFTBRACE,  0,             0,	         /* 56  [ {                      */
	SKEY_RIGHTBRACE, 0,             0,	         /* 57  ] }                      */
	SKEY_BACKSLASH,  0,             0,	         /* 58  \ |                      */
	SKEY_END,        0,             0,	         /* 59  Contacts  Word           */
	SKEY_U,          0,             SKEY_KP4,	 /* 60  u                  4     */
	SKEY_I,          0,             SKEY_KP5,	 /* 61  i                  5     */
	SKEY_O,          0,             SKEY_KP6,	 /* 62  o                  6     */
	SKEY_P,          0,             SKEY_KPASTERISK, /* 63  p                  *     */
	SKEY_APOSTROPHE, 0,             0,	         /* 64  ' "                      */
	SKEY_ENTER,      0,             0,	         /* 65  Enter     OK             */
	SKEY_PAGEUP,     0,             0,	         /* 66  Calendar  Excel          */
	0, 0, 0,	/* 67 */
	SKEY_J,          0,             SKEY_KP1,	 /* 68  j                  1     */
	SKEY_K,          0,             SKEY_KP2,      	 /* 69  k                  2     */
	SKEY_L,          0,             SKEY_KP3,	 /* 70  l                  3     */
	SKEY_SEMICOLON,  0,             SKEY_KPMINUS,    /* 71  ; :                -     */
	SKEY_SLASH,      0,             SKEY_KPPLUS,	 /* 72  / ?                +     */
	SKEY_UP,         SKEY_PAGEUP,   0,	         /* 73  up_arrow  PgUp           */
	SKEY_PAGEDOWN,   0,             0,	         /* 74  Tasks     Money          */
	0, 0, 0,	/* 75 */
	SKEY_M,          0,             SKEY_KP0,	 /* 76  m                  0     */
	SKEY_COMMA,      0,             SKEY_KPCOMMA,	 /* 77  , <                ,     */
	SKEY_DOT,        0,             SKEY_KPDOT,	 /* 78  . >                .     */
	SKEY_INSERT,     0,             0,               /* 79  Today                    */
	SKEY_DELETE,     0,             0,	         /* 80  Del                      */
	SKEY_LEFT,       SKEY_HOME,     0,	         /* 81  lt_arrow  Home           */
	SKEY_DOWN,       SKEY_PAGEDOWN, 0,	         /* 82  dn_arrow  PgDn           */
	SKEY_RIGHT,      SKEY_END,      0,	         /* 83  rt_arrow  End            */
	0, 0, 0,	/* 84 */
	0, 0, 0,	/* 85 */
	0, 0, 0,	/* 86 */
	0, 0, 0,        /* 87 */
	SKEY_LEFTSHIFT,  0,              0,	         /* 88  Shift(L)                 */
	SKEY_RIGHTSHIFT, 0,              0,	         /* 89  Shift(R)                 */
	0, 0, 0,	/* 90 */
	0, 0, 0,	/* 91 */
	0, 0, 0,	/* 92 */
	0, 0, 0,	/* 93 */
	0, 0, 0, 	/* 94 */
	0, 0, 0         /* 95 */
};

#define SKBD_PRESS	      0x7f
#define SKBD_RELEASE	      0x80
#define SKBD_KEYMAP_SIZE        96        /* Number of rows in the keymap */
#define SKBD_MODIFIER_STATES     3        /* Number of valid states (normal, function, numlock) */
#define SKBD_FUNCTION_OFFSET     1
#define SKBD_NUMLOCK_OFFSET      2
#define SKBD_MAX_KEYCODES      128        /* 128 possible keycodes */

#define SKBD_KEYCODE_TABLE_SIZE (SKBD_KEYMAP_SIZE * SKBD_MODIFIER_STATES)
#define SKBD_BITMAP_SIZE        SKBD_MAX_KEYCODES / BITS_PER_LONG

#define SKBD_STATE_INIT          0
#define SKBD_STATE_PREWAKE       1
#define SKBD_STATE_AWAKE         2
#define SKBD_STATE_SLEEP         3

#define SKBD_DEFAULT_PREWAKE_INTERVAL   HZ / 10   /* 100 milliseconds */
#define SKBD_DEFAULT_SLEEP_DELAY        5000      /* Five seconds     */

static int skbd_sleep_delay = SKBD_DEFAULT_SLEEP_DELAY;  /* Five seconds */
static int skbd_numlock     = 0;

struct skbd_state {
	unsigned char      keycode[SKBD_KEYCODE_TABLE_SIZE];
	unsigned long      key_down[SKBD_BITMAP_SIZE];
	int                function;   /* Flag for "function key is down" */
	int                state;      /* Current keyboard state (see SKBD_STATE_*) */
	int                dcd;
	struct timer_list  timer;
	void              (*old_ledfunc)(unsigned int led);
	int                usage_count;
};

struct skbd_statistics {
	u32   isr;          /* RX interrupts       */
	u32   rx;           /* Bytes received      */
	u32   frame;        /* Frame errors        */
	u32   overrun;      /* Overrun errors      */
	u32   parity;       /* Parity errors       */
	u32   dcd;          /* DCD interrupts       */
};

static struct skbd_statistics     g_statistics;
static struct skbd_state          g_keyboard;

#define SKBD_DLEVEL 1
#define SDEBUG(x, format, args...)  if ( x < SKBD_DLEVEL ) printk(format, ## args)
#define SFDEBUG(x, format, args...) if ( x < SKBD_DLEVEL ) printk(__FUNCTION__ ": " format, ## args )

/***********************************************************************************/
/*   Track keyboard press/releases and handle all state maintenance                */
/***********************************************************************************/

static void h3600_stowaway_release_all_keys(struct skbd_state *skbd)
{
	int i,j;
	unsigned long *b = skbd->key_down;

	SFDEBUG(1,"releasing all keys.\n"); 

	for ( i = 0 ; i < SKBD_MAX_KEYCODES ; i+=BITS_PER_LONG, b++ ) {
		if ( *b ) {  /* Should fix this to use the ffs() function */
			for (j=0 ; j<BITS_PER_LONG; j++)
				if (test_and_clear_bit(j,b))
					handle_scancode(i+j, 0);
		}
	}
}

static void h3600_stowaway_goto_state( struct skbd_state *skbd, int new_state )
{
	unsigned long flags;

	if ( !skbd || skbd->state == new_state )
		return;

	SFDEBUG(1,"switching from state %d to %d (%ld)\n", skbd->state,new_state,jiffies);

	save_flags(flags); cli();
	
	switch(new_state) {
	case SKBD_STATE_PREWAKE:
		GPSR |= GPIO_H3600_COM_RTS; /* Clear RTS */
		mod_timer(&skbd->timer, jiffies + SKBD_DEFAULT_PREWAKE_INTERVAL );
		break;
	case SKBD_STATE_AWAKE:
		GPCR |= GPIO_H3600_COM_RTS; /* Set RTS   */
		mod_timer(&skbd->timer, jiffies + (skbd_sleep_delay * HZ) / 1000);
		break;
	case SKBD_STATE_SLEEP:
		GPSR |= GPIO_H3600_COM_RTS; /* Clear RTS */
		del_timer(&skbd->timer);
		break;
	}
	skbd->state = new_state;
	restore_flags(flags);
}

static void h3600_stowaway_update_state_dcd_received( struct skbd_state *skbd )
{
	if ( !skbd ) return;

	switch (skbd->state) {
	case SKBD_STATE_PREWAKE:
		/* Do nothing...we will wake up soon anyway */
		break;
	case SKBD_STATE_AWAKE:           /* Keyboard was reseated       */
		h3600_stowaway_goto_state( skbd, SKBD_STATE_PREWAKE );   /* We need to flip RTS once    */
		break;
	case SKBD_STATE_SLEEP:
		h3600_stowaway_goto_state( skbd, SKBD_STATE_AWAKE );     /* A keypress happened */
		break;
	}
}

static void h3600_stowaway_update_state_char_received( struct skbd_state *skbd )
{
	unsigned long flags;
	if ( !skbd ) return;
	
	switch (skbd->state) {
	case SKBD_STATE_PREWAKE:
		/* Do nothing...we will wake up soon anyways */
		break;
	case SKBD_STATE_AWAKE:
		/* Reset our timeout */
		save_flags(flags); cli();
		mod_timer(&skbd->timer, jiffies + (skbd_sleep_delay * HZ) / 1000);
		restore_flags(flags);
		break;
	case SKBD_STATE_SLEEP:
		/* This shouldn't have happened */
		printk(KERN_ALERT __FUNCTION__ ": received character in sleep mode\n");
		h3600_stowaway_goto_state( skbd, SKBD_STATE_AWAKE );     /* Switch to keypress state */
		break;
	}
}

static void h3600_stowaway_update_state_timeout( struct skbd_state *skbd )
{
	if ( !skbd ) return;

	switch (skbd->state) {
	case SKBD_STATE_PREWAKE:
		h3600_stowaway_goto_state( skbd, SKBD_STATE_AWAKE );
		break;
	case SKBD_STATE_AWAKE:
		h3600_stowaway_goto_state( skbd, SKBD_STATE_SLEEP );
		break;
	case SKBD_STATE_SLEEP:
		/* This shouldn't have happened */
		printk(KERN_ALERT __FUNCTION__ ": received timeout in sleep mode\n");
		break;
	}
}

static void h3600_stowaway_timer_callback(unsigned long nr)
{
	h3600_stowaway_update_state_timeout( (struct skbd_state *)nr );
}

/*
 * Callback from keyboard.c
 * We use this update to keep track of the state of the numlock key.
 */

static void h3600_stowaway_ledfunc(unsigned int led)
{
	skbd_numlock = led & VC_NUMLOCK;
	SFDEBUG(1,"numlock set to %d\n",skbd_numlock);
}

/***********************************************************************************/
/*   Interrupt handlers                                                            */
/***********************************************************************************/

static void h3600_stowaway_handshake_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	struct skbd_state *skbd = (struct skbd_state *) dev_id;
	
	SFDEBUG(2,"handshake interrupt %p\n", skbd);
	
	if (!skbd)
		return;

	g_statistics.dcd++;

	switch (irq) {
	case IRQ_GPIO_H3600_COM_DCD:
		skbd->dcd++;
		h3600_stowaway_update_state_dcd_received( skbd );
		GEDR = GPIO_H3600_COM_DCD; /* Clear the interrupt */
		break;
	default:
		printk(KERN_ALERT __FILE__ ": Unknown IRQ %d\n", irq);
		break;
	}
}

static void h3600_stowaway_process_char(struct skbd_state *skbd, unsigned char data )
{
	unsigned char cooked;
	unsigned char *index;

	unsigned char key  = data & 0x7f;
	unsigned int  down = !(data & 0x80);

	SFDEBUG(1,"%#02x [%s]", key, (down?"down":" up "));

	if (key >= SKBD_KEYMAP_SIZE) {
		SDEBUG(1," --> discarding\n");
		return;
	}

	index = skbd->keycode + key * SKBD_MODIFIER_STATES;
	if ( *index == SKEY_FUNCTION ) {
		skbd->function = down;
		SDEBUG(1," --> function\n");
		return;
	}

	cooked = *index;
	if (skbd_numlock && index[SKBD_NUMLOCK_OFFSET])
		cooked = index[SKBD_NUMLOCK_OFFSET];
	if (skbd->function && index[SKBD_FUNCTION_OFFSET])
		cooked = index[SKBD_FUNCTION_OFFSET];
	
	if ( !cooked ) {
	        SDEBUG(1," --> invalid keystroke\n");
		return;
	}

	if ( (test_bit(cooked,skbd->key_down) && down)
	     || (!test_bit(cooked,skbd->key_down) && !down)) {
		SDEBUG(1," --> already set\n");
		return;
	}

	if ( down ) 
		set_bit(cooked,skbd->key_down);
	else
		clear_bit(cooked,skbd->key_down);

	SDEBUG(1," --> sending %d (%d)\n", cooked, down);
	handle_scancode(cooked, down);
}


static void h3600_stowaway_rx_chars( struct skbd_state *skbd )
{
	unsigned int status, ch;
	int count = 0;

	while ( (status = Ser3UTSR1) & UTSR1_RNE ) {
		ch = Ser3UTDR;
		g_statistics.rx++;

		if ( status & UTSR1_PRE ) { /* Parity error */
			g_statistics.parity++;
		} else 	if ( status & UTSR1_FRE ) { /* Framing error */
			g_statistics.frame++;
		} else {
			if ( status & UTSR1_ROR )   /* Overrun error */
				g_statistics.overrun++;
			
			count++;
			h3600_stowaway_process_char( skbd, ch );
		}
	}
	
	if ( count > 0 ) {
		h3600_stowaway_update_state_char_received(skbd);
		tasklet_schedule(&keyboard_tasklet); /* Run keyboard bottom half */
	}
}

static void h3600_stowaway_data_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	struct skbd_state *skbd = (struct skbd_state *) dev_id;
	
        unsigned int status;	/* UTSR0 */

	SFDEBUG(2,"data interrupt %p\n", skbd);

	g_statistics.isr++;

        status = Ser3UTSR0;

	if ( status & (UTSR0_RID | UTSR0_RFS) ) {
		if ( status & UTSR0_RID )
			Ser3UTSR0 = UTSR0_RID; /* Clear the Receiver IDLE bit */
		h3600_stowaway_rx_chars( skbd );
	}

	/* Clear break bits */
	if (status & (UTSR0_RBB | UTSR0_REB))
		Ser3UTSR0 = status & (UTSR0_RBB | UTSR0_REB);
}


/***********************************************************************************/
/*   Initialization and shutdown code                                              */
/***********************************************************************************/

static void h3600_stowaway_reset_comm( void )
{
	SFDEBUG(1,"initializing serial port\n");

	/* Set up interrupts */
	Ser3UTCR3 = 0;                                 /* Clean up CR3                  */
	Ser3UTCR0 = UTCR0_8BitData | UTCR0_1StpBit;    /* 8 bits, no parity, 1 stop bit */

	Ser3UTCR1 = 0;                                 /* Baud rate to 9600 bits/sec    */
        Ser3UTCR2 = 23;

	Ser3UTSR0 = 0xff;                              /* Clear SR0 */
        Ser3UTCR3 = UTCR3_RXE | UTCR3_RIE;             /* Enable receive interrupt */
}

static int h3600_stowaway_startup(struct skbd_state *skbd)
{
	unsigned long flags;
	int retval = 0;

	SFDEBUG(1,"\n");
	save_flags_cli(flags);

	h3600_stowaway_reset_comm();

	/* Direction registers */
	GPDR |= GPIO_H3600_COM_RTS;
	GPDR &= ~GPIO_H3600_COM_DCD;
	set_GPIO_IRQ_edge( GPIO_H3600_COM_DCD, GPIO_RISING_EDGE );

	/* Timer structure */
	init_timer(&skbd->timer);
	skbd->timer.function = h3600_stowaway_timer_callback;
	skbd->timer.data = (unsigned long) skbd;

	retval = request_irq(IRQ_GPIO_H3600_COM_DCD,
			     h3600_stowaway_handshake_interrupt,
			     SA_SHIRQ | SA_INTERRUPT | SA_SAMPLE_RANDOM,
			     "Stowaway DCD", (void *)skbd);

	if ( !retval )
		retval = request_irq( IRQ_Ser3UART, 
				      h3600_stowaway_data_interrupt,
				      SA_SHIRQ | SA_INTERRUPT | SA_SAMPLE_RANDOM,
				      "Stowaway data", (void *)skbd);
				      
	restore_flags(flags);
	return retval;
}

static void h3600_stowaway_shutdown(struct skbd_state * skbd)
{
	unsigned long	flags;
	
	SFDEBUG(1,"\n");
	del_timer(&skbd->timer);

	save_flags_cli(flags);
	free_irq(IRQ_GPIO_H3600_COM_DCD, (void *)skbd);
	free_irq(IRQ_Ser3UART, (void *)skbd);
	restore_flags(flags);
}


/***********************************************************************************/
/*   Basic driver code                                                             */
/***********************************************************************************/

static ssize_t h3600_stowaway_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
	SFDEBUG(1,"\n");

	current->state = TASK_INTERRUPTIBLE;
	while (!signal_pending(current))
		schedule();
	current->state = TASK_RUNNING;
	return 0;
}

static int h3600_stowaway_open( struct inode * inode, struct file * filp)
{
	struct skbd_state *skbd = (struct skbd_state *) &g_keyboard;
	int retval;

	SFDEBUG(1,"\n");

	if ( skbd->usage_count == 0 ) {
		SFDEBUG(1,"Setting up new keyboard\n");
		memset(skbd, 0, sizeof(struct skbd_state));

		/* Set up the interrupts */
		if ( (retval = h3600_stowaway_startup(skbd)) != 0 )
			return retval;

		/* Turn on power to the RS232 chip */
		set_h3600_egpio(IPAQ_EGPIO_RS232_ON);

		/* Initialize the keycode translation table */
		memcpy(skbd->keycode, skbd_keycode, SKBD_KEYCODE_TABLE_SIZE);

		/* Install the numlock keyboard feedback */
		skbd->old_ledfunc = kbd_ledfunc;
		kbd_ledfunc = h3600_stowaway_ledfunc;
		skbd_numlock = 0;  /* Actually, we should check the real keyboard */

		/* Initialize our state */
		h3600_stowaway_goto_state(skbd, SKBD_STATE_PREWAKE);
	}

	filp->private_data = skbd;

	skbd->usage_count++;
	MOD_INC_USE_COUNT;
	return 0;
}

static int h3600_stowaway_release(struct inode * inode, struct file * filp)
{
	struct skbd_state *skbd = (struct skbd_state *) filp->private_data;

	if (1) SFDEBUG(1,"\n");

	if ( --skbd->usage_count == 0 ) {
		SFDEBUG(1,"Closing down keyboard\n");
		h3600_stowaway_release_all_keys(skbd);
		h3600_stowaway_shutdown(skbd);
		kbd_ledfunc = skbd->old_ledfunc;
		clr_h3600_egpio(IPAQ_EGPIO_RS232_ON);
	}

	MOD_DEC_USE_COUNT;
	return 0;
}

struct file_operations stowaway_fops = {
	read:           h3600_stowaway_read,
	open:		h3600_stowaway_open,
	release:	h3600_stowaway_release,
};

/***********************************************************************************/
/*   Proc filesystem interface                                                     */
/***********************************************************************************/

static struct ctl_table h3600_stowaway_table[] = 
{
	{1, "sleep_delay", &skbd_sleep_delay, sizeof(int), 
	 0666, NULL, &proc_dointvec},
	{0}
};

static struct ctl_table h3600_stowaway_dir_table[] =
{
	{12, "stowaway", NULL, 0, 0555, h3600_stowaway_table},
	{0}
};

static struct ctl_table_header *h3600_stowaway_sysctl_header = NULL;
static struct proc_dir_entry   *proc_dir;

#define PRINT_DATA(x,s) \
	p += sprintf (p, "%-20s : %d\n", s, g_statistics.x)

int h3600_stowaway_proc_stats_read(char *page, char **start, off_t off,
				   int count, int *eof, void *data)
{
	char *p = page;
	int len;

	PRINT_DATA(dcd,        "DCD interrupts");
	PRINT_DATA(isr,        "Keyboard interrupts");
	PRINT_DATA(rx,         "Bytes received");
	PRINT_DATA(frame,      "Frame errors");
	PRINT_DATA(overrun,    "Overrun errors");
	PRINT_DATA(parity,     "Parity errors");
	p += sprintf(p,"%-20s : %d\n", "Usage count", g_keyboard.usage_count);

	len = (p - page) - off;
	if (len < 0)
		len = 0;

	*eof = (len <= count) ? 1 : 0;
	*start = page + off;

	return len;
}


/***********************************************************************************/
/*   Initialization code                                                           */
/***********************************************************************************/

/* 
   TODO:  We seem to survive suspend/resume - no doubt due to settings by the serial driver
   We might want to not rely on them.

   TODO:  Limit ourselves to a single reader at a time.
   
   TODO:  Have a PROC setting so you don't need a reader to keep alive....
          This is the "I want to be myself" mode of operation.
*/

static int            g_stowaway_major;  /* Dynamic major number */
static devfs_handle_t devfs_stowaway;

#define H3600_STOWAWAY_MODULE_NAME "stowaway"
#define H3600_STOWAWAY_DEVICE_NAME "stowaway"
#define H3600_STOWAWAY_PROC_STATS  "stowaway"

int __init h3600_stowaway_init_module( void )
{
        /* register our character device */
        SFDEBUG(0,"registering char device\n");

        g_stowaway_major = devfs_register_chrdev(0, H3600_STOWAWAY_MODULE_NAME, &stowaway_fops);
        if (g_stowaway_major < 0) {
                printk(KERN_ALERT __FUNCTION__ ": can't get major number\n");
                return g_stowaway_major;
        }

        devfs_stowaway = devfs_register( NULL, H3600_STOWAWAY_DEVICE_NAME, 
					 DEVFS_FL_DEFAULT, g_stowaway_major, 
					 0, S_IFCHR | S_IRUSR | S_IWUSR, &stowaway_fops, NULL );

	/* Register in /proc filesystem */
	create_proc_read_entry(H3600_STOWAWAY_PROC_STATS, 0, proc_dir, 
			       h3600_stowaway_proc_stats_read, NULL );

	h3600_stowaway_sysctl_header = register_sysctl_table(h3600_stowaway_dir_table, 0);

	return 0;
}

void __exit h3600_stowaway_cleanup_module( void )
{
	unregister_sysctl_table(h3600_stowaway_sysctl_header);
	remove_proc_entry(H3600_STOWAWAY_PROC_STATS, proc_dir);

	devfs_unregister( devfs_stowaway );
	devfs_unregister_chrdev( g_stowaway_major, H3600_STOWAWAY_MODULE_NAME );
}

module_init(h3600_stowaway_init_module);
module_exit(h3600_stowaway_cleanup_module);


