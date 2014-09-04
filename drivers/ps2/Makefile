#
# Makefile for the ps2 device drivers.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

subdir-y			+= mcfs
mod-subdirs			+= mcfs

O_TARGET			:= ps2.o
obj-y				:=
obj-m				:=
obj-n				:=
obj-				:=

export-objs			= mc.o pad.o

ps2pad-objs			:= pad.o
ps2js-objs			:= joystick.o
ps2cdvd-objs			:= cdvd.o cdvdaltdev.o cdvddev.o cdvdutil.o
ps2sd-objs			:= sd.o sdmixer.o sdcnv.o iopmem.o
ps2mc-objs			:= mc.o mcdir.o mcfile.o mcdev.o
ps2rm-objs			:= rm.o
ps2sysconf-objs			:= sysconf.o
ps2sysproc-objs			:= sysproc.o
ps2smap-objs			:= smap.o
ps2dev-objs			:= ps2dev.o ps2event.o ps2mem.o ps2dma.o ps2image.o

obj-$(CONFIG_PS2_PAD)		+= ps2pad.o
obj-$(CONFIG_PS2_PAD_JS)	+= ps2js.o
obj-$(CONFIG_PS2_CDVD)		+= ps2cdvd.o
obj-$(CONFIG_PS2_SD)		+= ps2sd.o
obj-$(CONFIG_PS2_MC)		+= ps2mc.o
obj-$(CONFIG_PS2_RM)		+= ps2rm.o
obj-$(CONFIG_PS2_SYSCONF)	+= ps2sysconf.o 
obj-$(CONFIG_PS2_SYSPROC)	+= ps2sysproc.o 
obj-$(CONFIG_PS2_ETHER_SMAP)	+= ps2smap.o
obj-$(CONFIG_PS2_PS2DEV)	+= ps2devmod.o
obj-y				+= mcfs/ps2mcfsarc.o

include $(TOPDIR)/Rules.make

ps2pad.o: $(ps2pad-objs)
	$(LD) -r -o $@ $(ps2pad-objs)
ps2js.o: $(ps2js-objs)
	$(LD) -r -o $@ $(ps2js-objs)
ps2cdvd.o: $(ps2cdvd-objs)
	$(LD) -r -o $@ $(ps2cdvd-objs)
ps2sd.o: $(ps2sd-objs)
	$(LD) -r -o $@ $(ps2sd-objs)
ps2mc.o: $(ps2mc-objs)
	$(LD) -r -o $@ $(ps2mc-objs)
ps2rm.o: $(ps2rm-objs)
	$(LD) -r -o $@ $(ps2rm-objs)
ps2sysconf.o: $(ps2sysconf-objs)
	$(LD) -r -o $@ $(ps2sysconf-objs)
ps2sysproc.o: $(ps2sysproc-objs)
	$(LD) -r -o $@ $(ps2sysproc-objs)
ps2smap.o: $(ps2smap-objs)
	$(LD) -r -o $@ $(ps2smap-objs)
ps2devmod.o: $(ps2dev-objs)
	$(LD) -r -o $@ $(ps2dev-objs)
