/*
 *  snsc_mpu300_fanctl.h
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

#ifndef _SNSC_MPU300_FANCTL_H
#define _SNSC_MPU300_FANCTL_H

int fanctl_set_fan_mode( unsigned int fan_number, unsigned int mode );
int fanctl_set_duty_cycle( unsigned int fan_number, unsigned int duty );
int fanctl_get_duty_cycle( unsigned int fan_number );
int fanctl_set_divisor( unsigned int fan_number, unsigned int divisor );
int fanctl_get_divisor( unsigned int fan_number );
int fanctl_set_preload( unsigned int fan_number, unsigned int preload );
int fanctl_get_preload( unsigned int fan_number );
int fanctl_get_tach_count( unsigned int fan_number );
int fanctl_get_fan_rpm( unsigned int fan_number );

#endif /* _SNSC_MPU300_FANCTL_H */

