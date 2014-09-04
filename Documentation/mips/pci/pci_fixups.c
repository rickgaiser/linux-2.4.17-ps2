/*
 *
 *  PCI fixups template
 *  Copyright 2001, Pete Popov, ppopov@pacbell.net
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
#include <linux/config.h>

#ifdef CONFIG_PCI

#include <linux/types.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/init.h>


/*
 * Fixup your resources here, if necessary. *Usually* you 
 * don't have to do anything here.
 * Called after pcibios_fixup() and pcibios_fixup_irqs().
 */
void __init pcibios_fixup_resources(struct pci_dev *dev)
{
}


/*
 * Any board or system controller fixups go here.
 * Now, this is called after the pci_auto code (if enabled) and
 * after the linux pci scan.
 */
void __init pcibios_fixup(void)
{
}


/* 
 * This is very board specific. You'll have to look at
 * each pci device and assign its interrupt number.
 * This function is called last of all the fixup functions.
 */
void __init pcibios_fixup_irqs(void)
{
	struct pci_dev *dev;

	pci_for_each_dev(dev) {
	}
}

unsigned int pcibios_assign_all_busses(void)
{
        return 0;
}
#endif
