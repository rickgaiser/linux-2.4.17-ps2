
/**
 * @file lx_io.h
 * Lexra specific io routines prototypes
 */

extern unsigned char lx_inb(const unsigned char *addr);
extern void lx_outb( unsigned char val, unsigned char *addr);
extern unsigned short lx_inw(const unsigned short *addr);
extern void lx_outw( unsigned short val, unsigned short *addr);
extern unsigned int lx_inl(const unsigned int *addr);
extern void lx_outl( unsigned int val, unsigned int *addr);
extern void lx_insl(unsigned int port, void *buffer, unsigned long count);
extern void lx_outsl(unsigned int port, const void *buffer, unsigned long count);
