/*
 *  linux/drivers/video/snsc_wp.c
 *
 *  Sony NSC Boot wallpaper
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

#include <linux/config.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/kd.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <video/fbcon.h>
#include <linux/tty.h>
#include <linux/smp.h>
#include <linux/init.h>
#include <linux/zlib.h>
#include "snsc_wp.h"

#ifdef CONFIG_WALLPAPER_STATIC
/* start and end address for .wallpaper section */
extern void * __wp_start, * __wp_end;
#endif

#ifdef CONFIG_WALLPAPER_BUFSIZE
#define WP_BUFSIZE  CONFIG_WALLPAPER_BUFSIZE
#endif

const char *msghead = "snsc_wp:";

/*
 *  zlib decompression
 */
static void*
zalloc(voidpf opaque, uInt items, uInt size)
{
        return kmalloc(items*size, GFP_KERNEL);
}

static void
zfree(voidpf opaque, voidpf address)
{
        kfree(address);
}

#ifdef CONFIG_WALLPAPER_LARGEBUF
static int
zlib_decompress(unsigned char *indata, unsigned char *outdata, 
                long insize, long *outsize)
{
        int err;
        z_stream strm;

        strm.zalloc = (alloc_func)zalloc;
        strm.zfree = (free_func)zfree;
        strm.opaque = (voidpf)0;

        strm.next_in = indata;
        strm.avail_in = insize;

        strm.next_out = outdata;
        strm.avail_out = *outsize;
        
        err = inflateInit(&strm);
        if (err != Z_OK) {
                printk("%s inflateInit error: err=%d\n", msghead, err);
                return -1;
        }

        for (;;) {
                err = inflate(&strm, Z_FINISH);
                if (err == Z_STREAM_END)
                        break;
                if (err != Z_OK) {
                        printk("%s inflate err=%d\n", msghead, err);
                        return -1;
                }
        }

        err = inflateEnd(&strm);
        if (err != Z_OK) {
                printk("%s inflateEnd error: err=%d\n", msghead, err);
                return -1;
        }

        *outsize = strm.total_out;
        
        return 0;
}
#else
static int
zlib_decompress_with_fbwrite(unsigned char *indata, long insize, long outsize, 
                             unsigned char *fb)
{
        int i,err;
        z_stream strm;
        unsigned char *outdata, *poutdata;

        /* allocate buffer */
        outdata = kmalloc(WP_BUFSIZE, GFP_KERNEL);
        if (outdata == NULL) {
                printk("%s cannot allocate buffer for decompressed data\n",msghead);
                return -1;
        }

        strm.zalloc = (alloc_func)zalloc;
        strm.zfree = (free_func)zfree;
        strm.opaque = (voidpf)0;

        strm.next_in = indata;
        strm.avail_in = insize;

        strm.next_out = outdata;
        strm.avail_out = WP_BUFSIZE;
        
        err = inflateInit(&strm);
        if (err != Z_OK) {
                printk("%s inflateInit error: err=%d\n", msghead, err);
                kfree(outdata);
                return -1;
        }

        while (strm.total_in < insize) {
                err = inflate(&strm, Z_NO_FLUSH);
                if (err == Z_STREAM_END)
                        break;
                if (err != Z_OK) {
                        printk("%s inflate err=%d\n", msghead, err);
                        kfree(outdata);
                        return -1;
                }

                if (strm.avail_out == 0) {
                        poutdata = outdata;
                        for (i = 0; i < WP_BUFSIZE; i++)
                                fb_writeb(*poutdata++, fb++);
                        strm.next_out = outdata;
                        strm.avail_out = WP_BUFSIZE;
                }
        }

        if (err != Z_STREAM_END)
                for (;;) {
                        err = inflate(&strm, Z_FINISH);
                        if (err == Z_STREAM_END)
                                break;
                        if (err != Z_OK) {
                                printk("%s inflate err=%d\n", msghead, err);
                                kfree(outdata);
                                return -1;
                        }
                        if (strm.avail_out == 0) {
                                poutdata = outdata;
                                for (i = 0; i < WP_BUFSIZE; i++)
                                        fb_writeb(*poutdata++, fb++);
                                strm.next_out = outdata;
                                strm.avail_out = WP_BUFSIZE;
                        }
                }

        poutdata = outdata;
        for (i = 0; i < WP_BUFSIZE - strm.avail_out; i++)
                fb_writeb(*poutdata++, fb++);

        err = inflateEnd(&strm);
        if (err != Z_OK) {
                printk("%s inflateEnd error: err=%d\n", msghead, err);
                kfree(outdata);
                return -1;
        }

        if (strm.total_out != outsize) {
                printk("%s total size of decompressed data(0x%lx) is invalid(0x%lx)\n",
                       msghead, strm.total_out, outsize);
                kfree(outdata);
                return -1;
        }

        kfree(outdata);
        
        return 0;
}
#endif /* CONFIG_WALLPAPER_LARGEBUF */


/* check bwp header */
static int
chk_bwpheader(struct display *p, struct bwp_header *header, int bwpdatalen)
{
        if ((header->magic[0] != BWP_MAGIC0)
            || (header->magic[1] != BWP_MAGIC1)
            || (header->magic[2] != BWP_MAGIC2)
            || (header->magic[3] != BWP_MAGIC3)) {
                printk("%s no bwp magic\n", msghead);
                return -1;
        }

        if ((p->var.xres_virtual != header->width)
            || (p->var.yres_virtual < header->height)
            || (p->var.bits_per_pixel != header->bpp)) {
                printk("%s don't match resolution or pixel format(fb:%dx%d,%dbpp != wp:%dx%d,%dbpp)\n",
                       msghead,
                       p->var.xres_virtual, p->var.yres_virtual, p->var.bits_per_pixel,
                       header->width, header->height, header->bpp);

                return -1;
        }

        if (header->type == BWP_TYPE_UNCOMPRESSED) {
                int csize = header->width * header->height * (header->bpp >> 3);
                if (csize != bwpdatalen - sizeof(struct bwp_header)) {
                        printk("%s don't match datalen\n", msghead);
                        return -1;
                }
        }

        return 0;
}

/*
 *  Show boot wallpaper
 */
int
fbcon_show_wallpaper(void)
{
        struct display *p = &fb_display[fg_console]; /* draw to vt in foreground */
        unsigned char  *fb = p->screen_base;
        struct         bwp_header *head;
        unsigned char  *bwp_start, *outdata;
        long           outlen, bwp_size;
        int            i, err;

#ifdef CONFIG_WALLPAPER_STATIC
        /* search wallpaper data */
        if (&__wp_start != &__wp_end) {
                bwp_start = (unsigned char *)&__wp_start;
                bwp_size = (unsigned long)&__wp_end - (unsigned long)&__wp_start;
        } else {
                printk("%s no wallpaper data is found\n", msghead);
                return 0;
        }
#else
#warning Only support statically linked wallpaper
        return 0;
#endif

        /* check bwp header */
        head = (struct bwp_header *)bwp_start;
        if (chk_bwpheader(p, head, bwp_size) != 0)
                return 0;

        outlen = head->width * head->height * (head->bpp >> 3);

        /* if data is comppressed, decompress it */
        if (head->type == BWP_TYPE_COMPRESSED) {
#ifdef CONFIG_WALLPAPER_LARGEBUF
                int order;
                unsigned char *pdata;

                order = get_order(outlen);
                outdata = (unsigned char *)__get_free_pages(GFP_KERNEL, order);
                if (outdata == NULL) {
                        printk("%s cannot allocate buffer for decompressed data\n",msghead);
                        return 0;
                }

                err = zlib_decompress(bwp_start + sizeof(struct bwp_header), outdata, 
                                      bwp_size - sizeof(struct bwp_header),
                                      &outlen);
                if (err != 0) {
                        free_pages((unsigned long)outdata, order);
                        return 0;
                }
                pdata = outdata;
                for (i = 0; i < outlen; i++)
                        fb_writeb(*pdata++, fb++);
                free_pages((unsigned long)outdata, order);
#else
                /* write to fb decompressing data with small buffer */
                err = zlib_decompress_with_fbwrite(bwp_start + sizeof(struct bwp_header),
                                                   bwp_size - sizeof(struct bwp_header),
                                                   outlen, fb);
                if (err != 0) {
                        return 0;
                }
#endif
        } else {
                /* uncompressed data */
                outdata = (unsigned char *)(bwp_start + sizeof(struct bwp_header));
                for (i = 0; i < outlen; i++)
                        fb_writeb(*outdata++, fb++);
        }

        return outlen;
}
