/*
 *  Convirt PPM file to Sony NSC boot wallpaper(bwp) file
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <zlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "snsc_wp.h"

#define DEFAULT_DEPTH   24
#define BUFSIZE         (3 * 1024)

static int  width, height, maxval, doublebytes;
static int  depth = DEFAULT_DEPTH;
static int  complevel = Z_DEFAULT_COMPRESSION;   /* compression level by zlib */
static char *inname = NULL;
static char *outname = NULL;
static FILE *infile = NULL;
static FILE *outfile = NULL;
static u_char *indata = NULL;
static u_char *outdata = NULL;

/* finalization functions */
static void
do_free(void)
{
        if (indata) {
                free(indata);
                indata = NULL;
        }
        if (outdata) {
                free(outdata);
                outdata = NULL;
        }
}

static void
do_exit(int code)
{
        if ((infile) && (infile != stdin))
                fclose(infile);
        if ((outfile) && (outfile != stdout))
                fclose(outfile);
        do_free();
        exit(code);
}

/*
 *  check PPM file 
 *    only supports PPM files which includes only one image
 */
static int
whitespace(char c)
{
        if ((c == ' ') || (c == '\t') || (c == '\r') || (c == '\n'))
                return 1;
        return 0;
}

static int
skip_until_nextline(FILE *file)
{
        int c;
        while ((c = fgetc(file)) != EOF) {
                if ((char)c == '\n')
                        break;
        }
        if (c == EOF)
                return -1;
        return 0;
}

static int
get_word(FILE *file, char *buf, int maxbytes)
{
        int i, c;
        char *pbuf = buf;

        /* first get rid of whitespace and comments before words */
        while ((c = fgetc(file)) != EOF) {
                if ((char)c == '#') {
                        if (skip_until_nextline(file) < 0)
                                return -1;
                } else {
                        if (!whitespace((char)c))
                                break;
                }
        }
        if (c == EOF)
                return -1;

#ifdef DEBUG
        fprintf(stderr," ** pos = %d '%c'\n", ftell(file), c);
#endif

        /* get a word */
        for (i = 0; i < maxbytes; i++) {
                if (whitespace((char)c))
                        break;

                if ((char)c == '#') {
                        /* encounter comments, so skip until end of line */
                        if (skip_until_nextline(file) < 0)
                                return -1;
                        break;
                } else
                        *pbuf++ = (char)c;

                /* next character */
                if ((c = fgetc(file)) == EOF)
                        return -1;
        }
        if (c == EOF) {
                return -1;
        }

        *pbuf = '\0';
        return (int)(pbuf - buf);
}

static int
check_ppm(FILE *file)
{
        int  n;
        char buf[21];
        long csize;

        /* check magic number */
        n = get_word(file, buf, 20);
        if ((n != 2) || (buf[0] != 'P') || (buf[1] != '6')) {
                fprintf(stderr,"no magic number in PPM file\n");
                return -1;
        }

        /* get width, height, maxval */
        if (get_word(file, buf, 20) <= 0) {
                fprintf(stderr,"unexpected EOF\n");
                return -1;
        }
        width = atoi(buf);
        if (get_word(file, buf, 20) <= 0)  {
                fprintf(stderr,"unexpected EOF\n");
                return -1;
        }
        height = atoi(buf);
        if (get_word(file, buf, 20) <= 0)  {
                fprintf(stderr,"unexpected EOF\n");
                return -1;
        }
        maxval = atoi(buf);
        if (maxval < 256)
                doublebytes = 0;
        else
                doublebytes = 1;

        /* XXX */
        if (doublebytes) {
                fprintf(stderr,"PPM file whose maxval is greater than 255 is not supported\n");
                return -1;
        }
        /* image data size */
        csize = width * height * 3;
        if (doublebytes)
                csize <<= 1;

        return csize;
}

/*
 *  write bwp header to output file
 */
static int
write_header(FILE *out, int w, int h, int depth)
{
        struct bwp_header head;
        int    err;

        memset((void *)&head, 0, sizeof(struct bwp_header));
        
        head.magic[0] = BWP_MAGIC0;
        head.magic[1] = BWP_MAGIC1;
        head.magic[2] = BWP_MAGIC2;
        head.magic[3] = BWP_MAGIC3;
        head.width  = w;
        head.height = h;
        if (complevel != 0)
                head.type = BWP_TYPE_COMPRESSED;
        else 
                head.type = BWP_TYPE_UNCOMPRESSED;
        head.bpp = depth;
        if (depth == 24) {
                head.red_offset = 16;
                head.green_offset = 8;
                head.blue_offset = 0;
        } else if (depth == 16) {
                head.red_offset = 11;
                head.green_offset = 5;
                head.blue_offset = 0;
        }

        err = fwrite(&head, sizeof(struct bwp_header), 1, out);
        if (err != 1) {
                fprintf(stderr,"write header error\n");
                return -1;
        }

        return 0;
}

#define QUANT(val, from, to)   {val = (u_char)((u_int)val * (to+1) / (from+1));}

/* convert data(len must be multiple of 3 */
static int
convert_data(u_char *buf, int len, int maxval, int depth)
{
        int i;
        u_char tmp;

        if (depth == 24) {
                for (i = 0; i < len; i+= 3) {
                        QUANT(buf[i], maxval, 255);
                        QUANT(buf[i+1], maxval, 255);
                        QUANT(buf[i+2], maxval, 255);

                        /* BGR -> RGB */
                        tmp = buf[i];
                        buf[i] = buf[i+2];
                        buf[i+2] = tmp;
                }

                return len;
        } else {
                u_short *pout = (u_short *)buf;

                for (i = 0; i < len; i+= 3) {
                        /* 24bpp -> 16bpp(5,6,5) */
                        QUANT(buf[i], maxval, 31);
                        QUANT(buf[i+1], maxval, 63);
                        QUANT(buf[i+2], maxval, 31);

                        /* BGR -> RGB */
                        *pout++ = (buf[i] << 11) | (buf[i+1] << 5) | buf[i+2];
                }

                return (u_int)pout - (u_int)buf;
        }

        return 0;
}

/* 
 *  process_data
 *    - read data from input file
 *    - convert in output format
 *    - write data to output file
 */
static int
process_data(FILE *in, FILE *out, int len, int depth, int maxval)
{
        int    nread, nwrite, tread, twrite, ncvt;

        /* allocate data buffer */
        indata = (u_char *)malloc(BUFSIZE);
        if (indata == NULL) {
                fprintf(stderr,"Cannot allocate data buffer\n");
                return -1;
        }

        tread = 0;
        twrite = 0;
        while (tread < len) {
                nread = fread(indata, 1, BUFSIZE, in);
                if (nread <= 0 || (nread % 3) != 0) {
                        fprintf(stderr,"fread error\n");
                        do_free();
                        return -1;
                }
                ncvt = convert_data(indata, nread, maxval, depth);

                nwrite = fwrite(indata, 1, ncvt, out);
                if (nwrite != ncvt) {
                        fprintf(stderr,"fwrite error\n");
                        do_free();
                        return -1;
                }
                tread += nread;
                twrite += nwrite;
        }

        do_free();
        return twrite;
}

/* 
 *  process_data with zlib compression
 *    - read data from input file
 *    - convert in output format
 *    - compress data
 *    - write data to output file
 */
static int
process_data_comp(FILE *in, FILE *out, int len, int depth, int maxval, int complevel)
{
        int    nread, nwrite, ncvt;
        int    err;
        z_stream strm;

        /* allocate input data buffer */
        indata = (u_char *)malloc(BUFSIZE);
        if (indata == NULL) {
                fprintf(stderr,"Cannot allocate data buffer\n");
                return -1;
        }

        /*  allocate buffer for compreesed data */
        outdata = (u_char *)malloc(BUFSIZE);
        if (outdata == NULL) {
                fprintf(stderr,"Cannot allocate data buffer\n");
                do_free();
                return -1;
        }

        /* setup for deflate */
        strm.zalloc = (alloc_func)0;
        strm.zfree = (free_func)0;
        strm.opaque = (voidpf)0;
        
        err = deflateInit(&strm, complevel);
        if (err != Z_OK) {
                fprintf(stderr,"deflateInit error %d\n", err);
                do_free();
                return -2;
        }
        strm.avail_in = 0;
        strm.next_out = outdata;
        strm.avail_out = BUFSIZE;

        /* set len to size of converted data */
        len = len * (depth >> 3) / 3;

        while (strm.total_in < len) {
                if (strm.avail_in == 0) {
                        /* input buf is empty, so provide more input data */
                        nread = fread(indata, 1, BUFSIZE, in);
                        if (nread <= 0 || (nread % 3) != 0) {
                                fprintf(stderr,"fread error\n");
                                do_free();
                                return -1;
                        }
                        ncvt = convert_data(indata, nread, maxval, depth);
                        strm.next_in = indata;
                        strm.avail_in = ncvt;
                }

                /* deflate ! */
                err = deflate(&strm, Z_NO_FLUSH);
                if (err != Z_OK) {
                        fprintf(stderr,"deflate with Z_NO_FLUSH error %d\n", err);
                        do_free();
                        return -2;
                }

                if (strm.avail_out == 0) {
                        /* output buf is full, so write output data and
                           make room in output buf */
                        nwrite = fwrite(outdata, 1, BUFSIZE, out);
                        if (nwrite != BUFSIZE) {
                                fprintf(stderr,"fwrite error\n");
                                do_free();
                                return -1;
                        }
                        strm.next_out = outdata;
                        strm.avail_out = BUFSIZE;
                }
        }

        for (;;) {
                err = deflate(&strm, Z_FINISH);
                if (err == Z_STREAM_END)
                        break;
                if (err != Z_OK) {
                        fprintf(stderr,"deflate with Z_FINISH error %d\n", err);
                        do_free();
                        return -2;
                }
                if (strm.avail_out == 0){
                        nwrite = fwrite(outdata, 1, BUFSIZE, out);
                        if (nwrite != BUFSIZE) {
                                fprintf(stderr,"fwrite error\n");
                                do_free();
                                return -1;
                        }
                        strm.next_out = outdata;
                        strm.avail_out = BUFSIZE;
                }
        }

        nwrite = fwrite(outdata, 1, BUFSIZE - strm.avail_out, out);
        if (nwrite != (BUFSIZE - strm.avail_out)) {
                fprintf(stderr,"fwrite error\n");
                do_free();
                return -1;
        }

        err = deflateEnd(&strm);
        if (err != Z_OK) {
                fprintf(stderr,"deflateEnd error: err=%d\n", err);
                do_free();
                return -2;
        }

        do_free();
        return strm.total_out;
}

static void
usage(void)
{
        fprintf(stderr, "usage: ppmtobwp\n");
        fprintf(stderr, "    [-d depth]      bpp of output image(default: 24)\n");
        fprintf(stderr, "    [-i infile]     input PPM file(default: stdin)\n");
        fprintf(stderr, "    [-o outfile]    output bwp file(default: stdout)\n");
        fprintf(stderr, "    [-z level]      compression level[0-9]\n");
        fprintf(stderr, "                    (0: no compression, 1: best speed, 9: best compression)\n");
}

static int
parse_options(int argc, char *argv[])
{
        int opt;
        while((opt = getopt(argc, argv, "i:o:d:z:")) != -1) {
                switch(opt) {
                case 'i':
                        inname = optarg;
                        break;
                case 'o':
                        outname = optarg;
                        break;
                case 'd':
                        depth = atoi(optarg);
                        break;
                case 'z':
                        complevel = atoi(optarg);
                        break;
                case ':':
                        fprintf(stderr,"option needs a value\n");
                        usage();
                        return 1;
                case '?':
                        fprintf(stderr,"Unkown option: %c\n", optopt);
                        usage();
                        return 1;
                }
        }
        if ((depth != 16) && (depth != 24)) {
                fprintf(stderr,"%dbpp is not supported\n", depth);
                return 1;
        }

        if ((complevel != Z_DEFAULT_COMPRESSION) &&
            ((complevel < Z_NO_COMPRESSION) || (complevel > Z_BEST_COMPRESSION))) {
                fprintf(stderr,"compression level %d is not supported\n", complevel);
                return 1;
        }
        return 0;
}

int
main(int argc, char *argv[])
{
        int    rval, insize, outsize;

        if ((rval = parse_options(argc, argv)) != 0)
                exit(rval);

        /* open files */
        if (inname) {
                if ((infile = fopen(inname, "r")) == NULL) {
                        fprintf(stderr,"Cannot open input file %s\n", inname);
                        do_exit(2);
                }
        } else {
                infile = stdin;
        }

        if (outname) {
                if ((outfile = fopen(outname, "w")) == NULL) {
                        fprintf(stderr,"Cannot open output file %s\n", outname);
                        do_exit(2);
                }
        } else {
                outfile = stdout;
        }

        /*
         *  check input PPM file
         */
        if ((insize = check_ppm(infile)) <= 0)
                do_exit(3+rval);

        fprintf(stderr,"Input PPM file: %dx%d maxval = %d\n", width, height, maxval);

        /* 
         *  write header to output file
         */
        if ((rval = write_header(outfile, width, height, depth)) != 0)
                exit(rval);

        /* 
         *  process data
         *  (read, convert, compress and write)
         */
        if (complevel != 0)
                outsize = process_data_comp(infile, outfile, insize, depth, maxval, complevel);
        else
                outsize = process_data(infile, outfile, insize, depth, maxval);
        if (outsize < 0)
                do_exit(rval);

        fprintf(stderr,"Output wallpaper file: %dbpp, ", depth);
        if (complevel == Z_DEFAULT_COMPRESSION)
                fprintf(stderr, "compressed(default level)\n");
        else if (complevel == 0)
                fprintf(stderr, "not compressed\n");
        else
                fprintf(stderr, "compressed(level %d)\n", complevel);

        do_exit(0);
}
