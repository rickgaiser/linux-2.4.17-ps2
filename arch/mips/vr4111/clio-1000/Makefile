#
# Makefile for the Vadem Clio 1000 / Sharp Mobilon Tripad PV-6000
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

.S.s:
	$(CPP) $(AFLAGS) $< -o $@
.S.o:
	$(CC) $(AFLAGS) -c $< -o $@

O_TARGET := clio-1000.o

all: clio-1000.o

obj-y	 := setup.o prom.o

include $(TOPDIR)/Rules.make
