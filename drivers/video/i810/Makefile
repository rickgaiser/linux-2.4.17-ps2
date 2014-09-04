#
# Makefile for the Intel 810/815 framebuffer driver
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...

O_TARGET := i810fb.o

export-objs                     := i810_main.o i810_accel.o i810_fbcon.o i810_sarea.o

obj-y				:= i810_main.o i810_accel.o i810_fbcon.o i810_sarea.o

ifdef CONFIG_FB_I810_GTF
obj-y                           += i810_nondiscrete.o
else
obj-y                           += i810_discrete.o
endif

obj-$(CONFIG_FB_I810_ACCEL)    	+= i810_fbcon_accel.o  
obj-$(CONFIG_FB_I810_ROTATE)    += i810_fbcon_rotate.o
obj-$(CONFIG_FB_I810_IFACE) 	+= i810_iface.o
obj-m	                        := $(O_TARGET)

include $(TOPDIR)/Rules.make
