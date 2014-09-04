/*
 *  snsc_mpu300_fanctl_ioctl.h
 *
 *  Copyright 2001,2002 Sony Corporation.
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

#ifndef _SNSC_MPU300_FANCTL_IOCTL_H
#define _SNSC_MPU300_FANCTL_IOCTL_H


#define FANCTL_MAGIC  'f'

#define FANCTL_SET_FANMODE     _IOW(FANCTL_MAGIC, 0, sizeof(int))
#define FANCTL_SET_DUTYCYCLE   _IOW(FANCTL_MAGIC, 1, sizeof(int))
#define FANCTL_GET_DUTYCYCLE   _IOR(FANCTL_MAGIC, 2, sizeof(int *))
#define FANCTL_SET_DIVISOR     _IOW(FANCTL_MAGIC, 3, sizeof(int))
#define FANCTL_GET_DIVISOR     _IOR(FANCTL_MAGIC, 4, sizeof(int *))
#define FANCTL_SET_PRELOAD     _IOW(FANCTL_MAGIC, 5, sizeof(int))
#define FANCTL_GET_PRELOAD     _IOR(FANCTL_MAGIC, 6, sizeof(int *))
#define FANCTL_GET_TACHCOUNT   _IOR(FANCTL_MAGIC, 7, sizeof(int *))
#define FANCTL_GET_FANRPM      _IOR(FANCTL_MAGIC, 8, sizeof(int *))


#endif /* _SNSC_MPU300_FANCTL_IOCTL_H */

