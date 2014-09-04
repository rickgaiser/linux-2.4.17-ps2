#
#  Copyright (C) 2002 Sony Corporation.
#
#  This program is based on arch/mips/ite-boards/qed-4n-s01b/Makefile.
#
#
#  Copyright 2000 MontaVista Software Inc.
#  Author: MontaVista Software, Inc.
#     	ppopov@mvista.com or source@mvista.com
#
# Makefile for the Sony NSC MPU-210 board, board
# specific files.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

.S.s:
	$(CPP) $(CFLAGS) $< -o $*.s
.S.o:
	$(CC) $(CFLAGS) -c $< -o $*.o

O_TARGET := snsc_mpu210.o

export-objs := init.o 
obj-y := init.o 

obj-$(CONFIG_PCI) += pci_fixup.o

include $(TOPDIR)/Rules.make
