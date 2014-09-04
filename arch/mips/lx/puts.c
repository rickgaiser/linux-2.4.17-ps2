/*
 *	Low level uart routines to directly access the uart.
 */

#include <linux/types.h>

extern void serial_console_putc(unsigned long device, char c) ;

static const char digits[16] = "0123456789abcdef";

void
puts(unsigned char *cp)
{
	while (*cp) {
		serial_console_putc(0, *cp); 
		*cp++;
	}
	serial_console_putc(0, '\r'); 
	serial_console_putc(0, '\n'); 
}

void
put32(unsigned u)
{
    int cnt;
    unsigned ch;

    cnt = 8;            /* 8 nibbles in a 32 bit long */
    serial_console_putc(0, '0');
    serial_console_putc(0, 'x');
    do {
        cnt--;
        ch = (unsigned char)(u >> cnt * 4) & 0x0F;
                serial_console_putc(0, digits[ch]);
    } while (cnt > 0);
}
