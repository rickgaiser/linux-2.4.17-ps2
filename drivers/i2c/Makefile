#
# Makefile for the kernel i2c bus driver.
#

O_TARGET := i2c.o

export-objs	:= i2c-core.o i2c-algo-bit.o i2c-algo-pcf.o i2c-proc.o i2c-algo-8xx.o i2c-algo-ppc405.o

obj-$(CONFIG_I2C)		+= i2c-core.o
obj-$(CONFIG_I2C_CHARDEV)	+= i2c-dev.o
obj-$(CONFIG_I2C_ALGOBIT)	+= i2c-algo-bit.o
obj-$(CONFIG_I2C_BITLP)		+= i2c-philips-par.o
obj-$(CONFIG_I2C_BITELV)	+= i2c-elv.o
obj-$(CONFIG_I2C_BITVELLE)	+= i2c-velleman.o
obj-$(CONFIG_I2C_ALGOPCF)	+= i2c-algo-pcf.o
obj-$(CONFIG_I2C_PCFISA)	+= i2c-elektor.o
obj-$(CONFIG_I2C_PROC)		+= i2c-proc.o
obj-$(CONFIG_I2C_ALGO8XX)	+= i2c-algo-8xx.o i2c-rpx.o
obj-$(CONFIG_I2C_ALGO8260)	+= i2c-algo-8260.o
obj-$(CONFIG_I2C_KEYWEST)	+= i2c-keywest.o
obj-$(CONFIG_I2C_PPC405_ALGO)	+= i2c-algo-ppc405.o
obj-$(CONFIG_I2C_PPC405_ADAP)	+= i2c-adap-ppc405.o

obj-$(CONFIG_I2C_PFS168)	+= i2c-pfs168.o
obj-$(CONFIG_I2C_M41T11)	+= m41t11.o

# This is needed for automatic patch generation: sensors code starts here
obj-$(CONFIG_I2C_ALI1535)		+= i2c-ali1535.o
obj-$(CONFIG_I2C_ALI15X3)		+= i2c-ali15x3.o
obj-$(CONFIG_I2C_AMD756)		+= i2c-amd756.o
obj-$(CONFIG_I2C_HYDRA)			+= i2c-hydra.o
obj-$(CONFIG_I2C_I801)			+= i2c-i801.o
obj-$(CONFIG_I2C_I810)			+= i2c-i810.o
obj-$(CONFIG_I2C_ISA)			+= i2c-isa.o
obj-$(CONFIG_I2C_PIIX4)			+= i2c-piix4.o
obj-$(CONFIG_I2C_SIS5595)		+= i2c-sis5595.o
obj-$(CONFIG_I2C_TSUNAMI)		+= i2c-tsunami.o
obj-$(CONFIG_I2C_VIA)			+= i2c-via.o
obj-$(CONFIG_I2C_VIAPRO)		+= i2c-viapro.o
obj-$(CONFIG_I2C_VOODOO3)		+= i2c-voodoo3.o
# This is needed for automatic patch generation: sensors code ends here

include $(TOPDIR)/Rules.make

