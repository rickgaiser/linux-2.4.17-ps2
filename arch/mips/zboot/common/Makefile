#
# arch/ppc/boot/common/Makefile
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Tom Rini	January 2001
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

clean:
	rm -rf *.o

OBJCOPY_ARGS = -O elf32-tradlittlemips

include $(TOPDIR)/Rules.make
