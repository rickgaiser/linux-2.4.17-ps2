/**
 * @file lx_io.c
 * Lexra specifice io routines.
 */
	
#include <asm/io.h>
#include <asm/byteorder.h>
#include <asm/addrspace.h>
#include <asm/lexra/lx_defs.h>

/**
 * Function for byte Input.
 * @param addr pointer to the address of the byte
 * @return the byte requested
 */
unsigned char lx_inb(const unsigned char *addr)
{
	unsigned int ia = (unsigned int)(addr);
	return *(volatile unsigned char *)KSEG1ADDR(ia);
}

/**
 * Function for byte Output.
 * @param val the byte of data to be output
 * @param addr pointer to the destination address of the byte
 */
void lx_outb( unsigned char val, unsigned char *addr)
{
	unsigned int ia = (unsigned int)(addr);
	*(volatile unsigned char *)KSEG1ADDR(ia) = val;
}

/**
 * Function for word (16 bit) Input.
 * @param addr pointer to the address of the word
 * @return the word requested
 */
unsigned short lx_inw(const unsigned short *addr)
{
	unsigned int ia = (unsigned int)(addr);

	if(PHYSADDR(ia) > LX_PCI_TOP)
		return *(volatile unsigned short *)KSEG1ADDR(ia);
	else
		return le16_to_cpu(*(volatile unsigned short *)KSEG1ADDR(ia));
}

/**
 * Function for word (16 bit) Output.
 * @param val the byte of word to be output
 * @param addr pointer to the destination address of the word
 */
void lx_outw( unsigned short val, unsigned short *addr)
{
	unsigned int ia = (unsigned int)(addr);

	if(PHYSADDR(ia) > LX_PCI_TOP)
		*(volatile unsigned short *)KSEG1ADDR(ia) = val;
	else
		*(volatile unsigned short *)
			(KSEG1ADDR(ia)) = cpu_to_le16(val);
}

/**
 * Function for long word (32 bit) Input.
 * @param addr pointer to the address of the long word
 * @return the long word requested
 */
unsigned int lx_inl(const unsigned int *addr)
{
	unsigned int ia = (unsigned int)(addr);

	if(PHYSADDR(ia) > LX_PCI_TOP)
		return *(volatile unsigned int *)KSEG1ADDR(ia);
	else
		return le32_to_cpu(*(volatile unsigned int *) KSEG1ADDR(ia));
}

/**
 * Function for long word (32 bit) Output.
 * @param val the long word of data to be output
 * @param addr pointer to the destination address of the long word
 */
void lx_outl( unsigned int val, unsigned int *addr)
{
	unsigned int ia = (unsigned int)(addr);

	if(PHYSADDR(ia) > LX_PCI_TOP)
		*(volatile unsigned int *)KSEG1ADDR(ia) = val;
	else
		*(volatile unsigned int *)KSEG1ADDR(ia) = cpu_to_le32(val);
}

/**
 * Function for multiple long word (32 bit) Input. Count long words
 * are read starting at the address specified by port and placed in
 * the location specified by buffer.
 * @param port Pointer to the address of the long words to be input.
 * @param buffer Pointer to the buffer for the input data.
 * @param count Number of words to input.
 */
void lx_insl(unsigned int port, void *buffer, unsigned long count)
{
	unsigned long *buf=buffer;
	while(count--) *buf++=inl(port);
}

/**
 * Function for multiple long word (32 bit) Output. Count long words
 * are read starting at the address specified by buffer and placed in
 * the location specified by port.
 * @param port Pointer to the destination address for the long words.
 * @param buffer The buffer of long words to be output.
 * @param count The number of long words to output.
 */
void lx_outsl(unsigned int port, const void *buffer, unsigned long count)
{
	const unsigned long *buf=buffer;
	while(count--) outl(*buf++, port);
}
