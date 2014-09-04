#
# Makefile for the kernel tracing drivers.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now inherited from the
# parent makes..
#

O_TARGET := trace_driver.o

# Is it loaded as a module or as part of the kernel
obj-$(CONFIG_TRACE) = tracer.o

include $(TOPDIR)/Rules.make
