#
# Makefile for the kernel serial device drivers.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now inherited from the
# parent makes..
#
#  $Id: Makefile,v 1.2 2001/10/12 15:46:58 rmk Exp $
#

O_TARGET := serial.o

export-objs	:= serial_core.o serial_8250.o
obj-y		:=
obj-m		:=
obj-n		:=
obj-		:=

serial-8250-y :=
serial-8250-$(CONFIG_PCI) += serial_8250_pci.o
serial-8250-$(CONFIG_ISAPNP) += serial_8250_pnp.o
obj-$(CONFIG_SERIAL_CORE) += serial_core.o
obj-$(CONFIG_SERIAL_21285) += serial_21285.o
obj-$(CONFIG_SERIAL_8250) += serial_8250.o $(serial-8250-y)
obj-$(CONFIG_SERIAL_ANAKIN) += serial_anakin.o
obj-$(CONFIG_SERIAL_AMBA) += serial_amba.o
obj-$(CONFIG_SERIAL_CLPS711X) += serial_clps711x.o
obj-$(CONFIG_SERIAL_SA1100) += serial_sa1100.o
obj-$(CONFIG_SERIAL_UART00) += serial_uart00.o
obj-$(CONFIG_SERIAL_IXP1200) += serial_ixp1200.o

include $(TOPDIR)/Rules.make

fastdep:

