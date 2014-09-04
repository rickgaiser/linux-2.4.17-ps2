#
# arch/ppc/boot/Makefile
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 1994 by Linus Torvalds
# Adapted for PowerPC by Gary Thomas
# modified by Cort (cort@cs.nmt.edu)
#

.c.s:
	$(CC) $(CFLAGS) -S -o $*.s $<
.s.o:
	$(AS) -o $*.o $<
.c.o:
	$(CC) $(CFLAGS) -c -o $*.o $<
.S.s:
	$(CPP) $(AFLAGS) -o $*.o $<
.S.o:
	$(CC) $(AFLAGS) -c -o $*.o $<

GZIP_FLAGS = -v9f

CFLAGS	:= $(CPPFLAGS) -O2 -D__BOOTER__ \
	-fomit-frame-pointer -fno-strict-aliasing -fno-common \
	-G 0 -mno-abicalls -fno-pic -mcpu=r4600 -mips2 \
		-I$(TOPDIR)/arch/$(ARCH)/zboot/include \
		-I$(TOPDIR)/include/asm
AFLAGS	+= -D__BOOTER__

lib/zlib.a:
	$(MAKE) -C lib

images/vmlinux.gz: $(TOPDIR)/vmlinux
	$(MAKE) -C images vmlinux.gz

BOOT_TARGETS = zImage

$(BOOT_TARGETS): lib/zlib.a images/vmlinux.gz
ifdef CONFIG_MIPS_PB1000
	$(MAKE) -C pb1000
endif
ifdef CONFIG_MIPS_PB1500
	$(MAKE) -C pb1500
endif

# Do the dirs
clean:
	$(MAKE) -C common clean
	$(MAKE) -C images clean
	$(MAKE) -C pb1000 clean
	$(MAKE) -C pb1500 clean

include $(TOPDIR)/Rules.make
