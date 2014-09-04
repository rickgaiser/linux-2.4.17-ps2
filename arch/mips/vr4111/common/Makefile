#
# Makefile for common code of NEC vr4111 based boards
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

.S.s:
	$(CPP) $(CFLAGS) $< -o $*.s
.S.o:
	$(CC) $(CFLAGS) -c $< -o $*.o

O_TARGET:= vr4111.o

obj-y	 := irq.o int-handler.o icu.o serial.o time.o power.o probe-ram.o

include $(TOPDIR)/Rules.make
