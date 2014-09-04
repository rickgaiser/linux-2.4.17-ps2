/*
 *  linux/include/linux/snsc_kpad.h  -- Sony NSC Keypad driver interface
 *
 *  Copyright 2002 Sony Corporation.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License.
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

#ifndef _LINUX_SNSC_KPAD_H_
#define _LINUX_SNSC_KPAD_H_

#include <linux/snsc_major.h>
#include <linux/ioctl.h>

#define SNSCKPAD_IOC_FLUSH	_IO('K', 0)

#ifdef __KERNEL__
struct keypad_dev {
	char	*name;
	int	 minor;
	int	(*open)(struct keypad_dev *dev);
	int	(*release)(struct keypad_dev *dev);
	int	(*ioctl)(struct inode *inode, struct file *file,
			 unsigned int cmd, unsigned long arg,
			 struct keypad_dev *dev);
	void	*reg;		/* do not access this member */
	void	*priv;		/* for your private data */
};

int register_keypad(struct keypad_dev *dev);
int unregister_keypad(struct keypad_dev *dev);
int keypad_event(struct keypad_dev *dev, u_int8_t event);
#endif /* __KERNEL__ */

#endif /* _LINUX_SNSC_KPAD_H_ */
