#
# Makefile for the ps2 device drivers.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

O_TARGET			:= ps2mcfsarc.o
obj-y				:=
obj-m				:=
obj-n				:=
obj-				:=

ps2mcfs-objs			:= mcfs.o inode.o root.o dir.o file.o \
				   null.o dirent.o path.o filedesc.o

obj-$(CONFIG_PS2_MCFS)		+= ps2mcfs.o

include $(TOPDIR)/Rules.make

ps2mcfs.o: $(ps2mcfs-objs)
	$(LD) -r -o $@ $(ps2mcfs-objs)
