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
 *
 *  $Id: sdmparse_conf.h,v 1.11.2.1 2002/11/08 11:30:33 takemura Exp $
 */

#ifndef _SDM_PARSE_CONF_H_
#define _SDM_PARSE_CONF_H_

#ifdef __KERNEL__

/* use CRC checking or not */
#define SDM_CRC_CHECK

/* support zlib decompression */
#define SDM_ZLIB_DECOMP
#define SDM_ZALLOC               0      /* use default func. */
#define SDM_ZFREE                0      /* use default func. */

/* adjust crc value to SDM */
/*
 * XXX: these macro currently uses JFFS2's crc32 routine, so it assumes that
 *      JFFS2 support is always enabled when sdmparse is used. 
 */
//#define sdm_crc32(buf, len)      (crc32(0xffffffff, buf, len) ^ 0xffffffff)
#define sdm_crc32(buf, len)      (crc32(0x0, buf, len))
#define gz_crc32(crc, buf, len)  (crc32(crc, buf, len))


#endif  /* __KERNEL__ */


/*
 * platform dependent settings
 */

#ifdef CONFIG_ARCH_DRAGONBALL
#include <asm/arch/mpu110_series.h>
#endif

/* MPU-210 */
#if defined(CONFIG_MTD_SNSC_MPU210) || defined(CONFIG_MTD_NAND_MPU210)
#define SDM_ENABLE_PARAM_ROOTNAME    /* sdm_image_desc has rootname field */
#define MAX_SDM_DEV               6  /* MPU-210 has 6 flash devices at most */
#define SDM_DEFAULT_MAXDESC      16  /* Maximum number of sdm descriptor */
#define SDM_MASTER_DEV            0  /* sdm table is located on device 0 */
#define SDM_TABLE_STARTSECT       0  /* sdm table starts from sector 0 */
#endif

/* verbose */
#define SDM_VERBOSE              1

/* parameter default setting */
#ifndef MAX_SDM_DEV
#warning "MAX_SDM_DEV is not defined in sdmparse_conf.h for current target platform, using default setting"
#define MAX_SDM_DEV              4
#endif
#ifndef SDM_DEFAULT_MAXDESC
#warning "SDM_DEFAULT_MAXDESC is not defined in sdmparse_conf.h for current target platform, using default setting"
#define SDM_DEFAULT_MAXDESC     16 
#endif
#ifndef SDM_MASTER_DEV
#warning "SDM_MASTER_DEV is not defined in sdmparse_conf.h for current target platform, using default setting"
#define SDM_MASTER_DEV            1
#endif
#ifndef SDM_TABLE_STARTSECT
#warning "SDM_TABLE_STARTSECT is not defined in sdmparse_conf.h for current target platform, using default setting"
#define SDM_TABLE_STARTSECT      0
#endif

#endif /* _SDM_PARSE_CONF_H_ */
