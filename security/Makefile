#
# Makefile for the kernel security code
#

# The target object and module list name.
O_TARGET	:= vmlinux-obj.o

# subdirectory list
subdir-$(CONFIG_LIDS)		+= lids 

# Objects that export symbols
export-objs	:= security.o capability.o

# if we don't select a security model, use the default capabilities
ifneq ($(CONFIG_SECURITY),y)
obj-y		+= capability.o
endif

# Object file lists
obj-$(CONFIG_SECURITY)			+= security.o dummy.o
obj-$(CONFIG_SECURITY_CAPABILITIES)	+= capability.o
obj-$(CONFIG_SECURITY_OWLSM)		+= owlsm.o
ifeq ($(CONFIG_LIDS),y)
	obj-y += lids/lids.o
endif

include $(TOPDIR)/Rules.make
