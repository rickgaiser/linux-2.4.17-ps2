/*
 *  NSC boot wallpaper
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

#ifndef __SNSC_WALLPAPER_H__
#define __SNSC_WALLPAPER_H__

#define BWP_HEADER_SIZE        64

struct bwp_header {
        u_int32_t  magic[4];     /* Magic number 16byte */
        u_int16_t  width;        /* image width         */
        u_int16_t  height;       /* image height        */
        u_int8_t   type;         /* image type          */
        u_int8_t   bpp;          /* bits per pixel      */
        u_int8_t   red_offset;   /* rgb offset          */
        u_int8_t   green_offset;
        u_int8_t   blue_offset;
        u_int8_t   reserved[BWP_HEADER_SIZE - 
                           (sizeof(u_int32_t)*4 + sizeof(u_int16_t)*2 
                            + sizeof(u_int8_t)*5)];
};

#define BWP_MAGIC0             0x52707753
#define BWP_MAGIC1             0x4941616e
#define BWP_MAGIC2             0x6d504c73
#define BWP_MAGIC3             0x47656c43
#define BWP_TYPE_UNCOMPRESSED  0
#define BWP_TYPE_COMPRESSED    1

#endif /* __SNSC_WALLPAPER_H__ */
