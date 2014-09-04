# arch/mips/compressed/alchemy/Makefile
# 
# Makefile for Alchemy Semiconductor PB1500 board.
# All of the boot loader code was derived from the ppc
# boot code.
#
# Copyright 2001,2002 MontaVista Software Inc.
#
# Author: Mark A. Greer
#	  mgreer@mvista.com
# Ported and modified for mips support by 
# Pete Popov <ppopov@mvista.com>
#
# This program is free software; you can redistribute  it and/or modify it
# under  the terms of  the GNU General  Public License as published by the
# Free Software Foundation;  either version 2 of the  License, or (at your
# option) any later version.

.c.s:
	$(CC) $(CFLAGS) -S -o $*.s $<
.s.o:
	$(AS) -o $*.o $<
.c.o:
	$(CC) $(CFLAGS) -D__BOOTER__ -c -o $*.o $<
.S.s:
	$(CPP) $(AFLAGS) -o $*.o $<
.S.o:
	$(CC) $(AFLAGS) -c -o $*.o $<

#########################################################################
# START BOARD SPECIFIC VARIABLES
BNAME=pb1500
# These two variables control where the zImage is stored
# in flash and loaded in memory. If you change either one,
# be sure to make the appropriate change to the zImage
# rule.
RAM_LOAD_ADDR = 0x81000000
FLASH_LOAD_ADDR = 0xBFD00000

# These two variables specify the free ram region
# that can be used for temporary malloc area
AVAIL_RAM_START=0x80400000
AVAIL_RAM_END=0x80800000

# This one must match the LOADADDR in arch/mips/Makefile!
LOADADDR=0x80100000
# END BOARD SPECIFIC VARIABLES
#########################################################################

ZLINKFLAGS = -T ld.script -Ttext $(RAM_LOAD_ADDR)

OBJECTS := head.o ../common/misc-common.o ../common/misc-simple.o \
	../common/au1k_uart.o ../common/string.o ../common/ctype.o
LIBS := ../lib/zlib.a

ENTRY := ../utils/entry
OFFSET := ../utils/offset
SIZE := ../utils/size

all: zImage

clean:
	rm -rf *.o vmlinux* zvmlinux.*

head.o: head.S $(TOPDIR)/vmlinux
	$(CC) -DFLASH_LOAD_ADDR=$(FLASH_LOAD_ADDR) $(AFLAGS) \
	-DKERNEL_ENTRY=$(shell sh $(ENTRY) $(NM) $(TOPDIR)/vmlinux ) \
	-c -o $*.o $<

../common/misc-simple.o:
	$(CC) $(CFLAGS) -DINITRD_OFFSET=0 -DINITRD_SIZE=0 -DZIMAGE_OFFSET=0 \
		-DAVAIL_RAM_START=$(AVAIL_RAM_START) \
		-DAVAIL_RAM_END=$(AVAIL_RAM_END) \
		-DLOADADDR=$(LOADADDR) \
		-DZIMAGE_SIZE=0 -c -o $@ $*.c

# This is the first pass at building the boot loader image,
# without knowing the file offset where the vmlinuz.gz
# kernel will end up.  We build this image, check the offset,
# and then rebuild it with the correct offset and size
# passed to mips-simple.c
zvmlinux.no: $(OBJECTS) $(LIBS) ../images/vmlinux.gz
	$(LD) $(ZLINKFLAGS) -o $@.tmp $(OBJECTS) $(LIBS)
	$(OBJCOPY) -R .comment \
		--add-section=image=../images/vmlinux.gz \
		$@.tmp $@
	# rm -f $@.tmp


# This is the final image we build, now that we know what
# the vmlinuz.gz offset is.
zvmlinux: $(OBJECTS) $(LIBS) ../images/vmlinux.gz zvmlinux.no
	$(CC) $(CFLAGS) -DINITRD_OFFSET=0 -DINITRD_SIZE=0 \
		-DZIMAGE_OFFSET=$(shell sh $(OFFSET) $(OBJDUMP) $@.no image) \
		-DZIMAGE_SIZE=$(shell sh $(SIZE) $(OBJDUMP) $@.no image) \
		-D__BOOTER__ \
		-DAVAIL_RAM_START=$(AVAIL_RAM_START) \
		-DAVAIL_RAM_END=$(AVAIL_RAM_END) \
		-DLOADADDR=$(LOADADDR) \
		-c -o ../common/misc-simple.o ../common/misc-simple.c
	$(LD) $(ZLINKFLAGS) -o $@.tmp $(OBJECTS) $(LIBS)
	$(OBJCOPY) -R .comment \
		--add-section=image=../images/vmlinux.gz \
		$@.tmp $@
	$(OBJCOPY) --adjust-section-vma=image+$(RAM_LOAD_ADDR) $@
	$(OBJCOPY) --adjust-section-vma=image+$(shell sh $(OFFSET) \
	$(OBJDUMP) $@.no image ) $@
	# rm -f $@.tmp
	# rm -f $@.no


# Here we manipulate the image in order to get it the necessary
# srecord file we need.
zImage: zvmlinux
	mv zvmlinux ../images/$@.$(BNAME)
	$(OBJCOPY) --set-section-flags=image=alloc,load,code ../images/$@.$(BNAME)
	$(OBJCOPY) -O srec --adjust-vma 0x3ed00000 \
	../images/$@.$(BNAME) ../images/$@.$(BNAME).srec
	# rm ../images/vmlinux.gz

include $(TOPDIR)/Rules.make
