#
# Makefile for VME bus support
#

O_TARGET		:= vme.o

export-objs		= vme_syms.o universe.o
obj-$(CONFIG_VMEBUS)	= vme_init.o copy_user_io.o vme_syms.o universe.o

include $(TOPDIR)/Rules.make