/*
 *
 * BRIEF MODULE DESCRIPTION
 *	Definitions for IDT 79S334 evaluation board.
 *
 * Copyright 2000,2001 MontaVista Software Inc.
 * Author: MontaVista Software, Inc.
 *         	stevel@mvista.com or source@mvista.com
 *
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

#ifndef _79S334_H_
#define _79S334_H_

#define IDT_BUS_FREQ   75 // MHz
#define IDT_CLOCK_MULT 2

/* NVRAM */
#define NVRAM_BASE         0x12000000
#define NVRAM_SIZE         512

/* LCD 4-digit display */
#define LCD_CLEAR          0x14000400
#define LCD_DIGIT0         0x1400000f
#define LCD_DIGIT1         0x14000008
#define LCD_DIGIT2         0x14000007
#define LCD_DIGIT3         0x14000003

extern int idtprintf(const char *fmt, ...);

/* Interrupts routed on 79S334A board (see rc32334.h) */
#define RC32334_SCC8530_IRQ  2
#define RC32334_PCI_INTA_IRQ 3
#define RC32334_PCI_INTB_IRQ 4
#define RC32334_PCI_INTC_IRQ 6
#define RC32334_PCI_INTD_IRQ 7

#define RAM_SIZE	(32*1024*1024)

#include <asm/rc32300/rc32334.h>

#endif /* _79S334_H_ */
