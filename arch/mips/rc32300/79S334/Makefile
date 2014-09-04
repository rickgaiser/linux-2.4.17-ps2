#
#  Copyright 2001 MontaVista Software Inc.
#  Author: MontaVista Software, Inc.
#     	stevel@mvista.com or source@mvista.com
#
# Makefile for the IDT 79S334 board specific parts of the kernel
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

.S.s:
	$(CPP) $(CFLAGS) $< -o $*.s
.S.o:
	$(CC) $(CFLAGS) -c $< -o $*.o

O_TARGET := idt-79S334.o

obj-y	 := irq.o prom.o setup.o
obj-$(CONFIG_PCI) += pci_fixup.o pci_ops.o

include $(TOPDIR)/Rules.make

