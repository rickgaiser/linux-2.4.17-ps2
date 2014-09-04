#
# Makefile for the MIPL Mobile IPv6 for Linux.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#


O_TARGET := mobile_ip6.o

obj-y :=	mipv6.o tunnel.o ha.o mn.o dstopts.o procrcv.o \
                mempool.o hashlist.o mdetect.o bul.o bcache.o \
                sendopts.o stats.o access.o sysctl.o router.o \
		dhaad.o halist.o ioctl.o util.o sadb.o ah_algo.o \
		auth_subopt.o multiaccess_ctl.o

ifeq ($(CONFIG_IPV6_MOBILITY_DEBUG),y)
obj-y += debug.o
endif

ifeq ($(CONFIG_IPV6_MOBILITY_AH),y)
obj-y += ah.o 
endif

obj-m	:= $(O_TARGET)

include $(TOPDIR)/Rules.make






