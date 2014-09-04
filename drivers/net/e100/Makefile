# File: drivers/net/e100/Makefile
#
# Makefile for the Intel e100 device driver.
#

O_TARGET := e100.o

obj-y  := e100_main.o e100_eeprom.o e100_config.o e100_phy.o e100_kcompat.o e100_test.o
obj-m  := $(O_TARGET)

EXTRA_CFLAGS += -I. -DLINUX -DEXPORT_SYMTAB -D__NO_VERSION__

ifeq ($(CONFIG_INTEL_E100_PROC),y)
obj-y += e100_proc.o
EXTRA_CFLAGS += -DE100_CONFIG_PROC_FS
endif

ifeq ($(CONFIG_INTEL_E100_IDIAG),y)
obj-y += idiag_e100.o
EXTRA_CFLAGS += -DE100_IDIAG_PRO_SUPPORT
endif

# XXX not used
ifeq ($(CONFIG_INTEL_E100_IANS),y)
obj-y += ans.o ans_hw.o ans_os.o ans_driver.o
EXTRA_CFLAGS += -DIANS -DIANS_BASE_ADAPTER_TEAMING \
              -DIANS_BASE_VLAN_TAGGING -DIANS_BASE_VLAN_ID
endif

include $(TOPDIR)/Rules.make

clean:
	rm -f core *.o *.a *.s
