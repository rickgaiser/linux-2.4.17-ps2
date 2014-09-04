#
# Makefile for the NEC MobilePro 780 PDA
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

.S.s:
	$(CPP) $(AFLAGS) $< -o $@
.S.o:
	$(CC) $(AFLAGS) -c $< -o $@

O_TARGET := mobilepro.o

all: mobilepro.o

obj-y	 := setup.o prom.o

include $(TOPDIR)/Rules.make
