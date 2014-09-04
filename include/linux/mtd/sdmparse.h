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
 *  $Id: sdmparse.h,v 1.4.4.1 2002/11/08 11:30:33 takemura Exp $
 */

#ifndef _SDM_PARSE_H_
#define _SDM_PARSE_H_

#include "sdmparse_conf.h"

typedef void *     sdmaddr_t;

#define SDM_IMGDESC_SIZE             512        /* total descriptor size      */
#define SDM_IMGNAME_LEN              16         /* image name length          */
#define SDM_IMGSTROPT_SIZE           256        /* image string option size   */

/* image descriptor */
struct sdm_image_desc {
        char       name[SDM_IMGNAME_LEN];       /* Null terminated name       */
        u_int32_t  start;                       /* Start sector of image      */
        sdmaddr_t  mem_base;                    /* Address in memory          */
        u_int32_t  sectors;                     /* Number of sectors          */
        sdmaddr_t  entry_point;                 /* Execution entry point      */
        u_int32_t  data_length;                 /* Length of actual data      */
        u_int32_t  magic;                       /* magic number               */
        u_int32_t  type;                        /* image type                 */
        u_int8_t   dev;                         /* device number              */
        u_int8_t   _rsvd[64-(SDM_IMGNAME_LEN 
                             + sizeof(sdmaddr_t) * 2 + sizeof(u_int32_t) * 6
                             + sizeof(u_int8_t) * 1)];
        u_int32_t  cksum;                       /* Checksum over image data   */
        char       stropt[SDM_IMGSTROPT_SIZE];  /* string option              */
        u_int32_t  cksum_stropt;                /* Checksum over stropt       */
        int8_t     max_retry;                   /* max retry count(for Linux) */
        u_int8_t   timeout;                     /* timeout (for Linux)        */
#ifdef SDM_ENABLE_PARAM_ROOTNAME
	char       rootname[SDM_IMGNAME_LEN];   /* root filesystem name       */
#else
        char       _rsvd_rootname[SDM_IMGNAME_LEN];
#endif
        u_int8_t   _rsvd2[SDM_IMGDESC_SIZE
                         - (SDM_IMGSTROPT_SIZE + 64 + sizeof(u_int32_t) 
			    + SDM_IMGNAME_LEN
                            + sizeof(u_int8_t) * 2)];
};

#define SDM_TYPE_LINUX               0x20

#define SDM_TABLE_DESC_OFFSET        SDM_IMGDESC_SIZE

/* sdm table */
struct sdm_table {
        u_int32_t  magic;                       /* Magic number               */
        u_int32_t  cksum;                       /* Checksum over sdm table    */
        u_int32_t  max_descs;                   /* Max number of img descs    */
        int32_t    version;                     /* SDM versoin                */
        char       def_img[SDM_IMGNAME_LEN];    /* default boot image name    */
        char       recov_img[SDM_IMGNAME_LEN];  /* recovery boot image name   */
        u_int8_t   _rsvd[SDM_TABLE_DESC_OFFSET
                        -(SDM_IMGNAME_LEN * 2 + sizeof(u_int32_t) * 4)];
        struct sdm_image_desc desc[0];
};

#define SDM_TABLE_MAGIC              0x53446973 /* SDis */
#define SDM_VERSION2                 2


/* error code */
#define SDM_OK                       0
#define SDM_ERR                      -1

#ifdef SDM_VERBOSE
#define SDM_INFO    printk
#else
#define SDM_INFO(arg...)
#endif

#endif /* _SDM_PARSE_H_ */
