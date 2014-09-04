#
# Makefile for common code of Sony's PlayStation 2
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

.S.s:
	$(CPP) $(CFLAGS) $< -o $*.s
.S.o:
	$(CC) $(CFLAGS) -c $< -o $*.o

O_TARGET := ps2.o

export-objs = siflock.o sbcall.o iopheap.o prom.o dma.o gs.o debuglog.o \
	      setup.o

obj-y       += int-handler.o irq.o reset.o time.o
obj-y       += powerbutton.o rtc.o pci.o
obj-y       += $(export-objs)

obj-$(CONFIG_PS2_SERIAL_CONSOLE)	+= romcons.o
obj-$(CONFIG_PS2_DEBUGLOG)		+= debuglog.o

include $(TOPDIR)/Rules.make

