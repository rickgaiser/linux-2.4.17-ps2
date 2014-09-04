/*
 * FILE NAME gpio.h
 *
 * BRIEF MODULE DESCRIPTION
 *	Generic gpio.
 *
 *  Armin Kuster akuster@mvista.com or source@mvista.com
 *  Sept, 2001
 *
 *  Orignial driver
 *  Author: MontaVista Software, Inc.  <source@mvista.com>
 *          Frank Rowand <frank_rowand@mvista.com>
 *
 * Copyright 2000 MontaVista Software Inc.
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

#ifndef __PPC405_GPIO_H
#define __PPC405_GPIO_H

#include <linux/ioctl.h>

#define	PPC405GPIO_IOCTL_BASE	'Z'

struct ppc405gpio_ioctl_data {
	__u32 device;
	__u32 mask;
	__u32 data;
};

#define GPIO_MINOR             185
#define PPC405GPIO_IN		_IOWR(PPC405GPIO_IOCTL_BASE, 0, struct ppc405gpio_ioctl_data)
#define PPC405GPIO_OUT		_IOW (PPC405GPIO_IOCTL_BASE, 1, struct ppc405gpio_ioctl_data)
#define PPC405GPIO_OPEN_DRAIN	_IOW (PPC405GPIO_IOCTL_BASE, 2, struct ppc405gpio_ioctl_data)
#define PPC405GPIO_TRISTATE	_IOW (PPC405GPIO_IOCTL_BASE, 3, struct ppc405gpio_ioctl_data)


#endif
