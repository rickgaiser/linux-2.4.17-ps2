/*
   --------------------------------------------------------------------
   i2c-ppc405.h: Global defines for the I2C controller on board the    
                 IBM 405 PPC processor.                                
   --------------------------------------------------------------------

   Ian DaSilva, MontaVista Software, Inc.
   idasilva@mvista.com or source@mvista.com

   Copyright 2000 MontaVista Software Inc.

 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.

 */


#ifndef I2C_PPC405_H
#define I2C_PPC405_H 1

/* Base Address */
#define PPC405GP_IICO_BASE	0xef600500

/* Registers */
#define IICO_MDBUF	0x0
#define IICO_SDBUF	0x2
#define IICO_LMADR	0x4
#define IICO_HMADR	0x5
#define IICO_CNTL	0x6
#define IICO_MDCNTL	0x7
#define IICO_STS	0x8
#define IICO_EXTSTS	0x9
#define IICO_LSADR	0xa
#define IICO_HSADR	0xb
#define IICO_CLKDIV	0xc
#define IICO_INTRMSK	0xd
#define IICO_XFRCNT	0xe
#define IICO_XTCNTLSS	0xf
#define IICO_DIRECTCNTL	0x10


#if 0
/*
 * IICO Control Register
 */

/* HMT */
#define IICO_NORMAL_TRANSFER	0x00
#define IICO_ISSUE_STOP		0x80

/* AMD */
#define IICO_SEVEN_BIT_ADDR	0x00
#define IICO_TEN_BIT_ADDR	0x40

/* TCT */
#define IICO_TRANSFER_1_BYTE	0x00
#define IICO_TRANSFER_2_BYTES	0x10
#define IICO_TRANSFER_3_BYTES	0x20
#define IICO_TRANSFER_4_BYTES	0x30

/* RPST */
#define IICO_NORMAL_START	0x00
#define IICO_REPEATED_START	0x08

/* CHT */
#define IICO_SINGLE_TRANSFER	0x00
#define IICO_SEQUENCE_TRANSFER	0x40

/* RW */
#define IICO_WRITE		0x00
#define IICO_READ		0x20

/* PT */
#define IICO_START_TRANSFER	0x01



/*
 * IICO Mode Control Register
 */

/* FSDB */
#define IICO_FLUSH_SLAVE_DATA_BUFFER	0x80

/* FMDB */
#define IICO_FLUSH_MASTER_DATA_BUFFER	0x40

/* EGC */
#define IICO_ENABLE_GENERAL_CALL	0x20

/* FSM */
#define IICO_STANDARD_MODE		0x00  /* Transfers at 100 kHz */
#define IICO_FAST_MODE			0x10  /* Transfers at 400 kHz */

/* ESM */
#define IICO_ENABLE_SLAVE_MODE		0x08

/* EINT */
#define IICO_ENABLE_INTERRUPTS		0x04

/* EUBS */
#define IICO_EXIT_UNKNOWN_BUS_STATE	0x02

/* HSCL */

#endif

#endif  /* I2C_PPC405_H */
