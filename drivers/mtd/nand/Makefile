#
# linux/drivers/nand/Makefile
#
# $Id: Makefile,v 1.7 2002/04/30 09:34:16 mag Exp $

O_TARGET	:= nandlink.o

export-objs	:= nand.o nand_ecc.o

nandobjs-y			:= nand.o
nandobjs-$(CONFIG_MTD_NAND_ECC) += nand_ecc.o

obj-$(CONFIG_MTD_NAND)		+= $(nandobjs-y)
obj-$(CONFIG_MTD_NAND_SPIA)	+= spia.o
obj-$(CONFIG_MTD_NAND_AUTCPU12)	+= autcpu12.o
obj-$(CONFIG_MTD_NAND_EDB7312)  += edb7312.o
obj-$(CONFIG_MTD_NAND_MPU110)   += snsc_mpu110-nand.o
obj-$(CONFIG_MTD_NAND_EBOOK)    += ebook-nand.o
obj-$(CONFIG_MTD_NAND_MPU200)	+= snsc_mpu200-nand.o
obj-$(CONFIG_MTD_NAND_MPU210)	+= snsc_mpu210-nand.o

include $(TOPDIR)/Rules.make
