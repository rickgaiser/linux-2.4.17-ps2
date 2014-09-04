#
#  Copyright 2001 MontaVista Software Inc.
#  Author: MontaVista Software, Inc.
#     	stevel@mvista.com or source@mvista.com
#
# Makefile for the IDT RC32300 MIPS core, generic files.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

.S.s:
	$(CPP) $(CFLAGS) $< -o $*.s
.S.o:
	$(CC) $(CFLAGS) -c $< -o $*.o

all: rc32300.o

O_TARGET := rc32300.o

obj-y := int-handler.o reset.o puts.o time.o idtdisplay.o

obj-$(CONFIG_REMOTE_DEBUG) += dbg_io.o             

include $(TOPDIR)/Rules.make
