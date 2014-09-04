# File: drivers/video/snsc_wpdata/Makefile
# Makefile for wallpaper data
#
# Copyright 2002 Sony Corporation.

include $(TOPDIR)/Rules.make

# for making dummy wallpaper data
O_FORMAT = $(shell $(OBJDUMP) -i | head -2 | grep elf32)
dummy_wp.o: wp.lds
	touch dummy.o
	$(LD) -T wp.lds -b binary --oformat $(O_FORMAT) -o $@ dummy.o

clean:
	rm -f core Makefile.exp *.o *.a *.s

#
# export files to make vmlinux from vmlinux.o and ppm image
#
EXPORT_DIR := $(TOPDIR)/export_wp

# XXX: add a file name of linker script if new arch is supported
ifeq ($(ARCH),arm)
	LINUXLDS = vmlinux.lds
endif
ifeq ($(ARCH),mips)
	LINUXLDS = ld.script
endif
ifeq ($(ARCH),i386)
	LINUXLDS = vmlinux.lds
endif

EXLINKFLAGS=$(shell echo $(LINKFLAGS) | sed -e 's/-T [a-z0-9/\.]*/-T $(LINUXLDS)/')

exportwp:  ppmtobwp wp.lds Makefile.exp $(TOPDIR)/arch/$(ARCH)/$(LINUXLDS)
	@rm -rf $(EXPORT_DIR)
	@mkdir -p $(EXPORT_DIR)
	install -m 644 wp.lds $(TOPDIR)/arch/$(ARCH)/$(LINUXLDS) $(TOPDIR)/vmlinux.o $(EXPORT_DIR)
	install -m 644 Makefile.exp $(EXPORT_DIR)/Makefile
	install ppmtobwp $(EXPORT_DIR)

ppmtobwp: ppmtobwp.c
	$(HOSTCC) -I.. $(HOSTCFLAGS) $< -o $@ -lz

Makefile.exp: Makefile.exp.in
	@sed -e 's!@@LD@@!$(LD)!; \
		s!@@NM@@!$(NM)!; \
		s!@@OBJDUMP@@!$(OBJDUMP)!; \
		s!@@OBJCOPY@@!$(OBJCOPY)!; \
		s!@@LINKFLAGS@@!$(EXLINKFLAGS)!;' \
		$< >$@

cleanwp:
	rm -rf $(EXPORT_DIR)
