#
#  Copyright (C) 2001,2002 Sony Corporation. All rights reserved.
#
#  Makefile for the Sony NSC MPU-200
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

.S.s:
	$(CPP) $(CFLAGS) $< -o $*.s
.S.o:
	$(CC) $(CFLAGS) -c $< -o $*.o

O_TARGET := snsc_mpu200.o

obj-y := init.o setup.o rtc-rx5c348.o rtc.o

obj-$(CONFIG_PCI) += pci_fixup.o pci_ops.o

include $(TOPDIR)/Rules.make
