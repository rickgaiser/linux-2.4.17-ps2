/*
 *
 *    Module name: 8390_redwood.h
 *
 *    Description:
 *	special hacks for 8390 STNIC on IBM Redwood board
 *
 */

#ifndef __8390_REDWOOD_H__
#define __8390_REDWOOD_H__

/*
  FTR made these routines, ostensibly to 'slow down' access the 8390
  chip.  I found it worked fine without them.  I'm not sure they are
  required...
*/

#undef	outb_p
#undef	inb_p
#undef	inb
#undef	outb

#define FTR_DELAY_HACK
#ifdef  FTR_DELAY_HACK

static inline unsigned char inb_p(long address)
{
	readb((void *)(address & 0xffffff00) + 0x40);
	return readb(address);
}

static inline void outb_p(unsigned char value, long address)
{
	readb((void *)(address & 0xffffff00) + 0x40);
	writeb(value, address);
}

static inline unsigned char inb(long address)
{
	readb((void *)(address & 0xffffff00) + 0x40);
	return readb(address);
}

static inline void outb(unsigned char value, long address)
{
	readb((void *)(address & 0xffffff00) + 0x40);
	writeb(value, address);
}

#else /* !FTR_DELAY_HACK */

#define	inb_p	readb
#define outb_p	writeb
#define	inb	readb
#define	outb	writeb

#endif /* FTR_DELAY_HACK */


#endif /* __8390_REDWOOD_H__ */

