#
# Makefile for the kernel pcmcia subsystem (c/o David Hinds)
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now inherited from the
# parent makes..

O_TARGET := pcmcia.o

export-objs := ds.o cs.o yenta.o pci_socket.o sa1100_h3600.o h3600_backpaq.o

list-multi := pcmcia_core.o yenta_socket.o

yenta_socket-objs := pci_socket.o yenta.o
pcmcia_core-objs := cistpl.o rsrc_mgr.o bulkmem.o cs.o

ifeq ($(CONFIG_CARDBUS),y)
  pcmcia_core-objs += cardbus.o
endif

ifeq ($(CONFIG_PCMCIA),y)
  obj-y   := cistpl.o rsrc_mgr.o bulkmem.o ds.o cs.o
  ifeq ($(CONFIG_CARDBUS),y)
    obj-y += cardbus.o yenta.o pci_socket.o
  endif
  ifeq ($(CONFIG_I82365),y)
    obj-y += i82365.o
  endif
  ifeq ($(CONFIG_I82092),y)
    obj-y += i82092.o
  endif
  ifeq ($(CONFIG_TCIC),y)
    obj-y += tcic.o
  endif
  ifeq ($(CONFIG_HD64465_PCMCIA),y)
    obj-y += hd64465_ss.o
  endif
else
  ifeq ($(CONFIG_PCMCIA),m)
    obj-m   := pcmcia_core.o ds.o
    ifeq ($(CONFIG_I82365),y)
      obj-m += i82365.o
    endif
    ifeq ($(CONFIG_I82092),y)
      obj-m += i82092.o
    endif
    ifeq ($(CONFIG_TCIC),y)
      obj-m += tcic.o
    endif
    ifeq ($(CONFIG_HD64465_PCMCIA),m)
      obj-m += hd64465_ss.o
    endif
     ifeq ($(CONFIG_CARDBUS),y)
	obj-m += yenta_socket.o
     endif
  endif
endif
obj-$(CONFIG_PCMCIA_M8XX)			+= m8xx_pcmcia.o
obj-$(CONFIG_PCMCIA_AU1000)			+= au1000_ss.o
obj-$(CONFIG_PCMCIA_AU1500)			+= au1500_ss.o
au1000_ss-objs-y				:= au1000_generic.o
au1000_ss-objs-$(CONFIG_MIPS_PB1000)		+= au1000_pb1000.o
au1000_ss-objs-$(CONFIG_MIPS_PB1500)		+= au1000_pb1500.o
au1000_ss-objs-$(CONFIG_SNSC_MPU200)		+= au1000_snsc_mpu200.o

obj-$(CONFIG_PCMCIA_CLPS6700)	+= clps6700.o
obj-$(CONFIG_PCMCIA_SA1100)	+= sa1100_cs.o
obj-$(CONFIG_MERCURY_BACKPAQ)   += h3600_backpaq.o
obj-$(CONFIG_H3600_SLEEVE)      += h3600_generic_sleeve.o

sa1100_cs-objs-y				:= sa1100_generic.o
sa1100_cs-objs-$(CONFIG_SA1100_ASSABET)		+= sa1100_assabet.o
sa1100_cs-objs-$(CONFIG_ASSABET_NEPONSET)	+= sa1100_neponset.o
sa1100_cs-objs-$(CONFIG_SA1100_H3XXX)		+= sa1100_h3600.o
sa1100_cs-objs-$(CONFIG_SA1100_CERF)		+= sa1100_cerf.o
sa1100_cs-objs-$(CONFIG_SA1100_GRAPHICSCLIENT)	+= sa1100_graphicsclient.o
sa1100_cs-objs-$(CONFIG_SA1100_XP860)		+= sa1100_xp860.o
sa1100_cs-objs-$(CONFIG_SA1100_PANGOLIN)	+= sa1100_pangolin.o
sa1100_cs-objs-$(CONFIG_SA1100_YOPY)		+= sa1100_yopy.o
sa1100_cs-objs-$(CONFIG_SA1100_SHANNON)		+= sa1100_shannon.o
sa1100_cs-objs-$(CONFIG_SA1100_FREEBIRD)	+= sa1100_freebird.o
sa1100_cs-objs-$(CONFIG_SA1100_PFS168)		+= sa1100_pfs168.o
sa1100_cs-objs-$(CONFIG_SA1100_JORNADA720)	+= sa1100_jornada720.o
sa1100_cs-objs-$(CONFIG_SA1100_FLEXANET)	+= sa1100_flexanet.o
sa1100_cs-objs-$(CONFIG_SA1100_SIMPAD)		+= sa1100_simpad.o
sa1100_cs-objs-$(CONFIG_SA1100_GRAPHICSMASTER)	+= sa1100_graphicsmaster.o
sa1100_cs-objs-$(CONFIG_SA1100_ADSBITSY)	+= sa1100_adsbitsy.o
sa1100_cs-objs-$(CONFIG_SA1100_STORK)		+= sa1100_stork.o

subdir-$(CONFIG_PCMCIA_PXA) += pxa
obj-$(CONFIG_PCMCIA_PXA)	+= pxa/pxa_cs.o

include $(TOPDIR)/Rules.make

pcmcia_core.o:  $(pcmcia_core-objs)
	$(LD) $(LD_RFLAG) -r -o $@ $(pcmcia_core-objs)

sa1100_cs.o: $(sa1100_cs-objs-y)
	$(LD) -r -o $@ $(sa1100_cs-objs-y)

au1000_ss.o: $(au1000_ss-objs-y)
	$(LD) -r -o $@ $(au1000_ss-objs-y)

au1500_ss.o: $(au1000_ss-objs-y)
	$(LD) -r -o $@ $(au1000_ss-objs-y)

yenta_socket.o: $(yenta_socket-objs)
	$(LD) $(LD_RFLAG) -r -o $@ $(yenta_socket-objs)
