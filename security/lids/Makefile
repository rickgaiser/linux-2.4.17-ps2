#
# Makefile for the LIDS code
#

O_TARGET :=  vmlinux-obj.o

EXTRA_CFLAGS += -Iinclude

obj-$(CONFIG_LIDS)	:= lids.o

lids-objs			:= lids_lsm.o lids_acl.o lids_exec.o lids_cap.o\
					lids_sysctl.o lids_init.o \
					lids_logs.o  rmd160.o 
ifeq ($(CONFIG_LIDS_SA_THROUGH_NET),y)
	lids-objs += lids_net.o
	lids-objs += klids.o
endif

include $(TOPDIR)/Rules.make

lids.o: $(lids-objs)
	$(LD) -r -o $@ $(lids-objs)
