/*
 *  npm.h -- Sony NSC Power Management module
 *
 */
/*
 *  Copyright 2002 Sony Corporation.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  version 2 of the  License.
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

#ifndef NPM_H_
#define NPM_H_

#include <linux/ioctl.h>

typedef struct npm_state_t {
	int            type;
	unsigned long  id;
	int            state;
} npm_state_t;

typedef struct npm_system_state_reg_t {
	int          num;
	npm_state_t *states;
} npm_system_state_reg_t;

typedef struct npm_find_t {
	char           name[16];
	int            type;
	unsigned long  id;
} npm_find_t;

#define NPM_IOC_STATE_CHANGE		_IOW('P', 0x00, npm_state_t)
#define NPM_IOC_SYSTEM_STATE_REGISTER	_IOW('P', 0x01, npm_system_state_reg_t)
#define NPM_IOC_SYSTEM_STATE_UNREGISTER	_IOW('P', 0x02, int)
#define NPM_IOC_SYSTEM_STATE_CHANGE	_IOW('P', 0x03, int)
#define NPM_IOC_FIND			_IOW('P', 0x04, npm_find_t)	

#endif /* !NPM_H_ */
