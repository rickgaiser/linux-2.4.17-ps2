#
# Makefile for common code of NEC Korva board
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

.S.s:
	$(CPP) $(CFLAGS) $< -o $*.s
.S.o:
	$(CC) $(CFLAGS) -c $< -o $*.o

O_TARGET:= korva.a

obj-y    := setup.o prom.o reset.o int_handler.o irq.o irq_cpu.o irq_korva.o \
		candy_setup.o

obj-$(CONFIG_REMOTE_DEBUG)      += dbg_io.o

export-objs := candy_setup.o

include $(TOPDIR)/Rules.make

