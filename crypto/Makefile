#
# Makefile for the Linux kernel crypto library.
#
# 1 Dec 2001, Herbert Valerio Riedel <hvr@kernel.org>
#

O_TARGET := crypto.o

export-objs = cryptoapi.o

obj-$(CONFIG_CRYPTO)           += cryptoapi.o

subdir-$(CONFIG_CRYPTOLOOP)    += cryptoloop
subdir-$(CONFIG_CIPHERS)       += ciphers
subdir-$(CONFIG_DIGESTS)       += digests

obj-y += $(join $(subdir-y),$(subdir-y:%=/%.o))

include $(TOPDIR)/Rules.make
