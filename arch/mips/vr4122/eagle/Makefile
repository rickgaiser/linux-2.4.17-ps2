#
# Copyright 2001,2002 MontaVista Software Inc.
# Author: Yoichi Yuasa
#	yyuasa@mvista.com or source@mvista.com
#
# Makefile for the NEC Eagle specific parts of the kernel
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

.S.s:
	$(CPP) $(AFLAGS) $< -o $@
.S.o:
	$(CC) $(AFLAGS) -c $< -o $@

O_TARGET := eagle.o

all: eagle.o

export-objs	:= vrc4173.o
obj-y		:= irq.o prom.o setup.o

obj-$(CONFIG_IDE)			+= ide-eagle.o
obj-$(CONFIG_PCI)			+= pci_fixup.o
obj-$(CONFIG_VRC4173)			+= vrc4173.o

include $(TOPDIR)/Rules.make
