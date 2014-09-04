#
# Makefile for the Lexra specific kernel interface routines
# under Linux.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...
#
# Note: for indenting files, use
#   indent -bl -bls -bad -bli0 -di4 -i4 -nlp

.S.s:
	$(CPP) $(CFLAGS) $< -o $*.s
.S.o:
	$(CC) $(CFLAGS) -c $< -o $*.o

all: lx4189.o

O_TARGET := lx4189.o
obj-y   := lx.o rtc.o irq.o int-handler.o lxpci.o lx_io.o lxserial.o lxRi.o puts.o

obj-$(CONFIG_REMOTE_DEBUG) += dbg_io.o             
clean:
	rm -f lx4189.o lx.o 

include $(TOPDIR)/Rules.make
