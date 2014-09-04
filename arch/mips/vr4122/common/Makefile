#
# Copyright 2001,2002 MontaVista Software Inc.
# Author: Yoichi Yuasa
#	yyuasa@mvista.com or source@mvista.com
#
# Makefile for the NEC Vr4122 CPU, generic files.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

.S.s:
	$(CPP) $(CFLAGS) $< -o $*.s
.S.o:
	$(CC) $(CFLAGS) -c $< -o $*.o

all: vr4122.o

O_TARGET := vr4122.o

obj-y := cmu.o icu.o int-handler.o reset.o

obj-$(CONFIG_VR4122_TIME_C) += time.o
obj-$(CONFIG_PCI) += pci_ops.o
obj-$(CONFIG_REMOTE_DEBUG) += dbg_io.o

include $(TOPDIR)/Rules.make
