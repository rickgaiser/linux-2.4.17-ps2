
/*
 * triple DES (DES EDE3) created from kerneli DES implementation by
 * Gisle Sælensminde <gisle@ii.uib.no>. Original DES sourcecode created
 * by descore. Descore is under GPL and made by Dana L. How
 * <how@isl.stanford.edu>, and modified by Raimar Falke
 * <rf13@inf.tu-dresden.de> for the Linux-Kernel.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This implementation accept 24, 16 or 8 byte keys for 3-key 3DES,
 * 2-key 3DES or DES compatibility mode respectivly. Other key lengths
 * are rejected. The parity bits, which is the least significant bit 
 * of each byte are always set to the correct value.
 *
 * In DES there are four weak and six pairs of semi-weak keys. For 3DES 
 * none of the keys should be weak, and no pair of semi-weak keys should 
 * appear among the three DES subkeys. If the key is (pseudo)random, the
 * chance of selecting a weak key is negligtable, and the test can safly
 * be omited. Such (pseudo)random keys are usually the result of a key
 * exchange protocol or the output of a cryptographic hash function. If
 * the key is selected by the user, the key can not be expected to be
 * random, and it can be desirable to check the keys. Since it's
 * possible for a user to directly select the key bits in some
 * applications of kerneli crypto API, we do check for weak keys.
 *
 * More specifically, a key is rejected if any of the DES subkeys are
 * weak or semi-weak. This is done because we reuse the check from the
 * original kerneli DES implementation. This function reject any key if
 * it is weak or semi-weak. As a result we may unnecessarily reject some
 * strong keys. In future releases this test should be improved, or
 * better; we should not let the user select the exact key bits, and
 * remove the need for checking.
 *
 * A DES_EDE3 key degenerates to DES if any of the DES subkeys K1 and
 * K2 or K2 and K3 are equal, even we have 112 key bits. This should be
 * avoided, unless we operate in DES compatibility mode. Again such keys
 * are unlikely if the key is random.  To have K1 = K3 is 2-key 3DES,
 * which not have such security problems. An exception from this policy
 * is done for DES compatibility mode, where all three keys are equal.
 */

#include <linux/string.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <asm/byteorder.h>
#include <linux/crypto.h>

#define ROR(d,c,o)      d = d >> c | d << o

typedef u8 DesData[8];
typedef u32 DesKeys[32];

static u32 des_keymap[] = {
 0x02080008, 0x02082000, 0x00002008, 0x00000000,
 0x02002000, 0x00080008, 0x02080000, 0x02082008,
 0x00000008, 0x02000000, 0x00082000, 0x00002008,
 0x00082008, 0x02002008, 0x02000008, 0x02080000,
 0x00002000, 0x00082008, 0x00080008, 0x02002000,
 0x02082008, 0x02000008, 0x00000000, 0x00082000,
 0x02000000, 0x00080000, 0x02002008, 0x02080008,
 0x00080000, 0x00002000, 0x02082000, 0x00000008,
 0x00080000, 0x00002000, 0x02000008, 0x02082008,
 0x00002008, 0x02000000, 0x00000000, 0x00082000,
 0x02080008, 0x02002008, 0x02002000, 0x00080008,
 0x02082000, 0x00000008, 0x00080008, 0x02002000,
 0x02082008, 0x00080000, 0x02080000, 0x02000008,
 0x00082000, 0x00002008, 0x02002008, 0x02080000,
 0x00000008, 0x02082000, 0x00082008, 0x00000000,
 0x02000000, 0x02080008, 0x00002000, 0x00082008,

 0x08000004, 0x00020004, 0x00000000, 0x08020200,
 0x00020004, 0x00000200, 0x08000204, 0x00020000,
 0x00000204, 0x08020204, 0x00020200, 0x08000000,
 0x08000200, 0x08000004, 0x08020000, 0x00020204,
 0x00020000, 0x08000204, 0x08020004, 0x00000000,
 0x00000200, 0x00000004, 0x08020200, 0x08020004,
 0x08020204, 0x08020000, 0x08000000, 0x00000204,
 0x00000004, 0x00020200, 0x00020204, 0x08000200,
 0x00000204, 0x08000000, 0x08000200, 0x00020204,
 0x08020200, 0x00020004, 0x00000000, 0x08000200,
 0x08000000, 0x00000200, 0x08020004, 0x00020000,
 0x00020004, 0x08020204, 0x00020200, 0x00000004,
 0x08020204, 0x00020200, 0x00020000, 0x08000204,
 0x08000004, 0x08020000, 0x00020204, 0x00000000,
 0x00000200, 0x08000004, 0x08000204, 0x08020200,
 0x08020000, 0x00000204, 0x00000004, 0x08020004,

 0x80040100, 0x01000100, 0x80000000, 0x81040100,
 0x00000000, 0x01040000, 0x81000100, 0x80040000,
 0x01040100, 0x81000000, 0x01000000, 0x80000100,
 0x81000000, 0x80040100, 0x00040000, 0x01000000,
 0x81040000, 0x00040100, 0x00000100, 0x80000000,
 0x00040100, 0x81000100, 0x01040000, 0x00000100,
 0x80000100, 0x00000000, 0x80040000, 0x01040100,
 0x01000100, 0x81040000, 0x81040100, 0x00040000,
 0x81040000, 0x80000100, 0x00040000, 0x81000000,
 0x00040100, 0x01000100, 0x80000000, 0x01040000,
 0x81000100, 0x00000000, 0x00000100, 0x80040000,
 0x00000000, 0x81040000, 0x01040100, 0x00000100,
 0x01000000, 0x81040100, 0x80040100, 0x00040000,
 0x81040100, 0x80000000, 0x01000100, 0x80040100,
 0x80040000, 0x00040100, 0x01040000, 0x81000100,
 0x80000100, 0x01000000, 0x81000000, 0x01040100,

 0x04010801, 0x00000000, 0x00010800, 0x04010000,
 0x04000001, 0x00000801, 0x04000800, 0x00010800,
 0x00000800, 0x04010001, 0x00000001, 0x04000800,
 0x00010001, 0x04010800, 0x04010000, 0x00000001,
 0x00010000, 0x04000801, 0x04010001, 0x00000800,
 0x00010801, 0x04000000, 0x00000000, 0x00010001,
 0x04000801, 0x00010801, 0x04010800, 0x04000001,
 0x04000000, 0x00010000, 0x00000801, 0x04010801,
 0x00010001, 0x04010800, 0x04000800, 0x00010801,
 0x04010801, 0x00010001, 0x04000001, 0x00000000,
 0x04000000, 0x00000801, 0x00010000, 0x04010001,
 0x00000800, 0x04000000, 0x00010801, 0x04000801,
 0x04010800, 0x00000800, 0x00000000, 0x04000001,
 0x00000001, 0x04010801, 0x00010800, 0x04010000,
 0x04010001, 0x00010000, 0x00000801, 0x04000800,
 0x04000801, 0x00000001, 0x04010000, 0x00010800,

 0x00000400, 0x00000020, 0x00100020, 0x40100000,
 0x40100420, 0x40000400, 0x00000420, 0x00000000,
 0x00100000, 0x40100020, 0x40000020, 0x00100400,
 0x40000000, 0x00100420, 0x00100400, 0x40000020,
 0x40100020, 0x00000400, 0x40000400, 0x40100420,
 0x00000000, 0x00100020, 0x40100000, 0x00000420,
 0x40100400, 0x40000420, 0x00100420, 0x40000000,
 0x40000420, 0x40100400, 0x00000020, 0x00100000,
 0x40000420, 0x00100400, 0x40100400, 0x40000020,
 0x00000400, 0x00000020, 0x00100000, 0x40100400,
 0x40100020, 0x40000420, 0x00000420, 0x00000000,
 0x00000020, 0x40100000, 0x40000000, 0x00100020,
 0x00000000, 0x40100020, 0x00100020, 0x00000420,
 0x40000020, 0x00000400, 0x40100420, 0x00100000,
 0x00100420, 0x40000000, 0x40000400, 0x40100420,
 0x40100000, 0x00100420, 0x00100400, 0x40000400,

 0x00800000, 0x00001000, 0x00000040, 0x00801042,
 0x00801002, 0x00800040, 0x00001042, 0x00801000,
 0x00001000, 0x00000002, 0x00800002, 0x00001040,
 0x00800042, 0x00801002, 0x00801040, 0x00000000,
 0x00001040, 0x00800000, 0x00001002, 0x00000042,
 0x00800040, 0x00001042, 0x00000000, 0x00800002,
 0x00000002, 0x00800042, 0x00801042, 0x00001002,
 0x00801000, 0x00000040, 0x00000042, 0x00801040,
 0x00801040, 0x00800042, 0x00001002, 0x00801000,
 0x00001000, 0x00000002, 0x00800002, 0x00800040,
 0x00800000, 0x00001040, 0x00801042, 0x00000000,
 0x00001042, 0x00800000, 0x00000040, 0x00001002,
 0x00800042, 0x00000040, 0x00000000, 0x00801042,
 0x00801002, 0x00801040, 0x00000042, 0x00001000,
 0x00001040, 0x00801002, 0x00800040, 0x00000042,
 0x00000002, 0x00001042, 0x00801000, 0x00800002,

 0x10400000, 0x00404010, 0x00000010, 0x10400010,
 0x10004000, 0x00400000, 0x10400010, 0x00004010,
 0x00400010, 0x00004000, 0x00404000, 0x10000000,
 0x10404010, 0x10000010, 0x10000000, 0x10404000,
 0x00000000, 0x10004000, 0x00404010, 0x00000010,
 0x10000010, 0x10404010, 0x00004000, 0x10400000,
 0x10404000, 0x00400010, 0x10004010, 0x00404000,
 0x00004010, 0x00000000, 0x00400000, 0x10004010,
 0x00404010, 0x00000010, 0x10000000, 0x00004000,
 0x10000010, 0x10004000, 0x00404000, 0x10400010,
 0x00000000, 0x00404010, 0x00004010, 0x10404000,
 0x10004000, 0x00400000, 0x10404010, 0x10000000,
 0x10004010, 0x10400000, 0x00400000, 0x10404010,
 0x00004000, 0x00400010, 0x10400010, 0x00004010,
 0x00400010, 0x00000000, 0x10404000, 0x10000010,
 0x10400000, 0x10004010, 0x00000010, 0x00404000,

 0x00208080, 0x00008000, 0x20200000, 0x20208080,
 0x00200000, 0x20008080, 0x20008000, 0x20200000,
 0x20008080, 0x00208080, 0x00208000, 0x20000080,
 0x20200080, 0x00200000, 0x00000000, 0x20008000,
 0x00008000, 0x20000000, 0x00200080, 0x00008080,
 0x20208080, 0x00208000, 0x20000080, 0x00200080,
 0x20000000, 0x00000080, 0x00008080, 0x20208000,
 0x00000080, 0x20200080, 0x20208000, 0x00000000,
 0x00000000, 0x20208080, 0x00200080, 0x20008000,
 0x00208080, 0x00008000, 0x20000080, 0x00200080,
 0x20208000, 0x00000080, 0x00008080, 0x20200000,
 0x20008080, 0x20000000, 0x20200000, 0x00208000,
 0x20208080, 0x00008080, 0x00208000, 0x20200080,
 0x00200000, 0x20000080, 0x20008000, 0x00000000,
 0x00008000, 0x00200000, 0x20200080, 0x00208080,
 0x20000000, 0x20208000, 0x00000080, 0x20008080,
};

static u8 rotors[] = {
 34, 13,  5, 46, 47, 18, 32, 41, 11, 53, 33, 20,
 14, 36, 30, 24, 49,  2, 15, 37, 42, 50,  0, 21,
 38, 48,  6, 26, 39,  4, 52, 25, 12, 27, 31, 40,
  1, 17, 28, 29, 23, 51, 35,  7,  3, 22,  9, 43,

 41, 20, 12, 53, 54, 25, 39, 48, 18, 31, 40, 27,
 21, 43, 37,  0,  1,  9, 22, 44, 49,  2,  7, 28,
 45, 55, 13, 33, 46, 11,  6, 32, 19, 34, 38, 47,
  8, 24, 35, 36, 30,  3, 42, 14, 10, 29, 16, 50,

 55, 34, 26, 38, 11, 39, 53,  5, 32, 45, 54, 41,
 35,  2, 51, 14, 15, 23, 36,  3,  8, 16, 21, 42,
  6, 12, 27, 47, 31, 25, 20, 46, 33, 48, 52,  4,
 22,  7, 49, 50, 44, 17,  1, 28, 24, 43, 30,  9,

 12, 48, 40, 52, 25, 53, 38, 19, 46,  6, 11, 55,
 49, 16, 10, 28, 29, 37, 50, 17, 22, 30, 35,  1,
 20, 26, 41,  4, 45, 39, 34, 31, 47,  5, 13, 18,
 36, 21,  8,  9,  3,  0, 15, 42,  7,  2, 44, 23,

 26,  5, 54, 13, 39, 38, 52, 33, 31, 20, 25, 12,
  8, 30, 24, 42, 43, 51,  9,  0, 36, 44, 49, 15,
 34, 40, 55, 18,  6, 53, 48, 45,  4, 19, 27, 32,
 50, 35, 22, 23, 17, 14, 29,  1, 21, 16,  3, 37,

 40, 19, 11, 27, 53, 52, 13, 47, 45, 34, 39, 26,
 22, 44,  7,  1,  2, 10, 23, 14, 50,  3,  8, 29,
 48, 54, 12, 32, 20, 38,  5,  6, 18, 33, 41, 46,
  9, 49, 36, 37,  0, 28, 43, 15, 35, 30, 17, 51,

 54, 33, 25, 41, 38, 13, 27,  4,  6, 48, 53, 40,
 36,  3, 21, 15, 16, 24, 37, 28,  9, 17, 22, 43,
  5, 11, 26, 46, 34, 52, 19, 20, 32, 47, 55, 31,
 23,  8, 50, 51, 14, 42,  2, 29, 49, 44,  0, 10,

 11, 47, 39, 55, 52, 27, 41, 18, 20,  5, 38, 54,
 50, 17, 35, 29, 30,  7, 51, 42, 23,  0, 36,  2,
 19, 25, 40, 31, 48, 13, 33, 34, 46,  4, 12, 45,
 37, 22,  9, 10, 28,  1, 16, 43,  8,  3, 14, 24,

 18, 54, 46,  5,  6, 34, 48, 25, 27, 12, 45,  4,
  2, 24, 42, 36, 37, 14,  3, 49, 30,  7, 43,  9,
 26, 32, 47, 38, 55, 20, 40, 41, 53, 11, 19, 52,
 44, 29, 16, 17, 35,  8, 23, 50, 15, 10, 21,  0,

 32, 11, 31, 19, 20, 48,  5, 39, 41, 26,  6, 18,
 16,  7,  1, 50, 51, 28, 17,  8, 44, 21,  2, 23,
 40, 46,  4, 52, 12, 34, 54, 55, 38, 25, 33, 13,
  3, 43, 30,  0, 49, 22, 37,  9, 29, 24, 35, 14,

 46, 25, 45, 33, 34,  5, 19, 53, 55, 40, 20, 32,
 30, 21, 15,  9, 10, 42,  0, 22,  3, 35, 16, 37,
 54, 31, 18, 13, 26, 48, 11, 12, 52, 39, 47, 27,
 17,  2, 44, 14,  8, 36, 51, 23, 43,  7, 49, 28,

 31, 39,  6, 47, 48, 19, 33, 38, 12, 54, 34, 46,
 44, 35, 29, 23, 24,  1, 14, 36, 17, 49, 30, 51,
 11, 45, 32, 27, 40,  5, 25, 26, 13, 53,  4, 41,
  0, 16,  3, 28, 22, 50, 10, 37,  2, 21,  8, 42,

 45, 53, 20,  4,  5, 33, 47, 52, 26, 11, 48, 31,
  3, 49, 43, 37,  7, 15, 28, 50,  0,  8, 44, 10,
 25,  6, 46, 41, 54, 19, 39, 40, 27, 38, 18, 55,
 14, 30, 17, 42, 36,  9, 24, 51, 16, 35, 22,  1,

  6, 38, 34, 18, 19, 47,  4, 13, 40, 25,  5, 45,
 17,  8,  2, 51, 21, 29, 42,  9, 14, 22,  3, 24,
 39, 20, 31, 55, 11, 33, 53, 54, 41, 52, 32, 12,
 28, 44,  0,  1, 50, 23,  7, 10, 30, 49, 36, 15,

 20, 52, 48, 32, 33,  4, 18, 27, 54, 39, 19,  6,
  0, 22, 16, 10, 35, 43,  1, 23, 28, 36, 17,  7,
 53, 34, 45, 12, 25, 47, 38, 11, 55, 13, 46, 26,
 42,  3, 14, 15,  9, 37, 21, 24, 44,  8, 50, 29,

 27,  6, 55, 39, 40, 11, 25, 34,  4, 46, 26, 13,
  7, 29, 23, 17, 42, 50,  8, 30, 35, 43, 24, 14,
 31, 41, 52, 19, 32, 54, 45, 18,  5, 20, 53, 33,
 49, 10, 21, 22, 16, 44, 28,  0, 51, 15,  2, 36,
};

static char parity[] = {
8,1,0,8,0,8,8,0,0,8,8,0,8,0,2,8,0,8,8,0,8,0,0,8,8,0,0,8,0,8,8,3,
0,8,8,0,8,0,0,8,8,0,0,8,0,8,8,0,8,0,0,8,0,8,8,0,0,8,8,0,8,0,0,8,
0,8,8,0,8,0,0,8,8,0,0,8,0,8,8,0,8,0,0,8,0,8,8,0,0,8,8,0,8,0,0,8,
8,0,0,8,0,8,8,0,0,8,8,0,8,0,0,8,0,8,8,0,8,0,0,8,8,0,0,8,0,8,8,0,
0,8,8,0,8,0,0,8,8,0,0,8,0,8,8,0,8,0,0,8,0,8,8,0,0,8,8,0,8,0,0,8,
8,0,0,8,0,8,8,0,0,8,8,0,8,0,0,8,0,8,8,0,8,0,0,8,8,0,0,8,0,8,8,0,
8,0,0,8,0,8,8,0,0,8,8,0,8,0,0,8,0,8,8,0,8,0,0,8,8,0,0,8,0,8,8,0,
4,8,8,0,8,0,0,8,8,0,0,8,0,8,8,0,8,5,0,8,0,8,8,0,0,8,8,0,8,0,6,8,
};

static int des_part_set_key(u32 *method, const unsigned char *key);

static int des_ede3_set_key(struct cipher_context *cx, 
			    const unsigned char *key, int keybytes, int atomic)
{
    u32 *method;
    int status,i;
    unsigned char lkey[24];
    unsigned char n1, n2;
    method=(u32 *)cx->keyinfo;
    
    /* asign keybits based on keylength */
    switch(keybytes) {
    case 24:
	    memcpy(lkey,key,24);
	    break;
    case 16:
	    memcpy(lkey,key,16);
	    memcpy(lkey+16,key,8);
	    break;
    case 8:
	    memcpy(lkey,key,8);
	    memcpy(lkey+8,key,8);
	    memcpy(lkey+16,key,8);
	    break;
    default:
	    return -EINVAL;
    }


#if 0
    /* I don't think these codes is necessary. Kazunori Miyazawa */
    /* set the correct parity bit for each byte in the key*/
    for(i=0; i<24; i++){
	    n1 = lkey[i] & 0xfe;
	    n2 = n1 ^ (n1 >> 4);
	    n2 ^= (n2 >> 2);
	    n2 ^= (n2 >> 1);
	    lkey[i] = n1 | (~n2 & 0x01);	    
    }
#endif 
    
    /* check for degenerate keys */
    if(keybytes > 8 &&
       (memcmp(lkey,lkey+8,8)==0 || memcmp(lkey+8,lkey+16,8)==0))
	    return -2;

    if((status = des_part_set_key(method, lkey)) != 0) 
	    return status;
    if((status = des_part_set_key(method+32, lkey+8)) != 0) 
	    return status;
    if((status = des_part_set_key(method+64, lkey+16)) != 0) 
	    return status;
    
    return 0;
}

static int des_part_set_key(u32 *method, const unsigned char *key)
{
    register u32 n, w;
    register char * b0, * b1;
    char bits0[56], bits1[56];

    /* check for weak keys */
    b0 = parity;
    n  = b0[key[0]]; n <<= 4;
    n |= b0[key[1]]; n <<= 4;
    n |= b0[key[2]]; n <<= 4;
    n |= b0[key[3]]; n <<= 4;
    n |= b0[key[4]]; n <<= 4;
    n |= b0[key[5]]; n <<= 4;
    n |= b0[key[6]]; n <<= 4;
    n |= b0[key[7]];
    w  = 0X88888888L;
    if ( !((n - (w >> 3)) & w) ) {  /* 1 in 10^10 keys passes this test */
	  if ( n < 0X41415151 ) {
            if ( n < 0X31312121 ) {
                if ( n < 0X14141515 ) {
                    /* 01 01 01 01 01 01 01 01 */
                    if ( n == 0X11111111 ) return -2;
                    /* 01 1F 01 1F 01 0E 01 0E */
                    if ( n == 0X13131212 ) return -2;
                } else {
                    /* 01 E0 01 E0 01 F1 01 F1 */
                    if ( n == 0X14141515 ) return -2;
                    /* 01 FE 01 FE 01 FE 01 FE */
                    if ( n == 0X16161616 ) return -2;
                }
            } else {
                if ( n < 0X34342525 ) {
                    /* 1F 01 1F 01 0E 01 0E 01 */
                    if ( n == 0X31312121 ) return -2;
                    /* 1F 1F 1F 1F 0E 0E 0E 0E */   /* ? */
                    if ( n == 0X33332222 ) return -2;
                } else {
                    /* 1F E0 1F E0 0E F1 0E F1 */
                    if ( n == 0X34342525 ) return -2;
                    /* 1F FE 1F FE 0E FE 0E FE */
                    if ( n == 0X36362626 ) return -2;
                }
            }
        } else {
            if ( n < 0X61616161 ) {
                if ( n < 0X44445555 ) {
                    /* E0 01 E0 01 F1 01 F1 01 */
                    if ( n == 0X41415151 ) return -2;
                    /* E0 1F E0 1F F1 0E F1 0E */
                    if ( n == 0X43435252 ) return -2;
                } else {
                    /* E0 E0 E0 E0 F1 F1 F1 F1 */   /* ? */
                    if ( n == 0X44445555 ) return -2;
                    /* E0 FE E0 FE F1 FE F1 FE */
                    if ( n == 0X46465656 ) return -2;
                }
            } else {
                if ( n < 0X64646565 ) {
                    /* FE 01 FE 01 FE 01 FE 01 */
                    if ( n == 0X61616161 ) return -2;
                    /* FE 1F FE 1F FE 0E FE 0E */
                    if ( n == 0X63636262 ) return -2;
                } else {
                    /* FE E0 FE E0 FE F1 FE F1 */
                    if ( n == 0X64646565 ) return -2;
                    /* FE FE FE FE FE FE FE FE */
                    if ( n == 0X66666666 ) return -2;
                }
            }
        }
    }

    /* explode the bits */
    n = 56;
    b0 = bits0;
    b1 = bits1;
    do {
        w = (256 | *key++) << 2;
        do {
            --n;
            b1[n] = 8 & w;
            w >>= 1;
            b0[n] = 4 & w;
        } while ( w >= 16 );
    } while ( n );

    /* put the bits in the correct places */
    n = 16;
    key = rotors;
    do {
        w   = (b1[key[ 0   ]] | b0[key[ 1   ]]) << 4;
        w  |= (b1[key[ 2   ]] | b0[key[ 3   ]]) << 2;
        w  |=  b1[key[ 4   ]] | b0[key[ 5   ]];
        w <<= 8;
        w  |= (b1[key[ 6   ]] | b0[key[ 7   ]]) << 4;
        w  |= (b1[key[ 8   ]] | b0[key[ 9   ]]) << 2;
        w  |=  b1[key[10   ]] | b0[key[11   ]];
        w <<= 8;
        w  |= (b1[key[12   ]] | b0[key[13   ]]) << 4;
        w  |= (b1[key[14   ]] | b0[key[15   ]]) << 2;
        w  |=  b1[key[16   ]] | b0[key[17   ]];
        w <<= 8;
        w  |= (b1[key[18   ]] | b0[key[19   ]]) << 4;
        w  |= (b1[key[20   ]] | b0[key[21   ]]) << 2;
        w  |=  b1[key[22   ]] | b0[key[23   ]];

        method[0] = w;

        w   = (b1[key[ 0+24]] | b0[key[ 1+24]]) << 4;
        w  |= (b1[key[ 2+24]] | b0[key[ 3+24]]) << 2;
        w  |=  b1[key[ 4+24]] | b0[key[ 5+24]];
        w <<= 8;
        w  |= (b1[key[ 6+24]] | b0[key[ 7+24]]) << 4;
        w  |= (b1[key[ 8+24]] | b0[key[ 9+24]]) << 2;
        w  |=  b1[key[10+24]] | b0[key[11+24]];
        w <<= 8;
        w  |= (b1[key[12+24]] | b0[key[13+24]]) << 4;
        w  |= (b1[key[14+24]] | b0[key[15+24]]) << 2;
        w  |=  b1[key[16+24]] | b0[key[17+24]];
        w <<= 8;
        w  |= (b1[key[18+24]] | b0[key[19+24]]) << 4;
        w  |= (b1[key[20+24]] | b0[key[21+24]]) << 2;
        w  |=  b1[key[22+24]] | b0[key[23+24]];

        ROR(w, 4, 28);      /* could be eliminated */
        method[1] = w;

        key += 48;
        method  += 2;
    } while ( --n );
    return 0;
}

static void DesSmallFipsEncrypt (DesData d, DesKeys r, DesData s)
{
    register u32 x, y, z;

    x  = s [7];
    x <<= 8;
    x |= s [6];
    x <<= 8;
    x |= s [5];
    x <<= 8;
    x |= s [4];
    y  = s [3];
    y <<= 8;
    y |= s [2];
    y <<= 8;
    y |= s [1];
    y <<= 8;
    y |= s [0];
    z  = ((x >> 004) ^ y) & 0X0F0F0F0FL;
    x ^= z << 004;
    y ^= z;
    z  = ((y >> 020) ^ x) & 0X0000FFFFL;
    y ^= z << 020;
    x ^= z;
    z  = ((x >> 002) ^ y) & 0X33333333L;
    x ^= z << 002;
    y ^= z;
    z  = ((y >> 010) ^ x) & 0X00FF00FFL;
    y ^= z << 010;
    x ^= z;
    x  = x >> 1 | x << 31;
    z  = (x ^ y) & 0X55555555L;
    y ^= z;
    x ^= z;
    y  = y >> 1 | y << 31;
    z  = r [0];
    z ^= y;
    x ^= * (u32 *) ((u8 *) (des_keymap + 192) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 128) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 64) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) des_keymap + (0XFC & z));
    z  = r [1];
    z ^= y;
    z  = z << 4 | z >> 28;
    x ^= * (u32 *) ((u8 *) (des_keymap + 448) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 384) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 320) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 256) + (0XFC & z));
    z  = r [2];
    z ^= x;
    y ^= * (u32 *) ((u8 *) (des_keymap + 192) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 128) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 64) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) des_keymap + (0XFC & z));
    z  = r [3];
    z ^= x;
    z  = z << 4 | z >> 28;
    y ^= * (u32 *) ((u8 *) (des_keymap + 448) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 384) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 320) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 256) + (0XFC & z));
    z  = r [4];
    z ^= y;
    x ^= * (u32 *) ((u8 *) (des_keymap + 192) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 128) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 64) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) des_keymap + (0XFC & z));
    z  = r [5];
    z ^= y;
    z  = z << 4 | z >> 28;
    x ^= * (u32 *) ((u8 *) (des_keymap + 448) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 384) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 320) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 256) + (0XFC & z));
    z  = r [6];
    z ^= x;
    y ^= * (u32 *) ((u8 *) (des_keymap + 192) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 128) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 64) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) des_keymap + (0XFC & z));
    z  = r [7];
    z ^= x;
    z  = z << 4 | z >> 28;
    y ^= * (u32 *) ((u8 *) (des_keymap + 448) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 384) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 320) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 256) + (0XFC & z));
    z  = r [8];
    z ^= y;
    x ^= * (u32 *) ((u8 *) (des_keymap + 192) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 128) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 64) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) des_keymap + (0XFC & z));
    z  = r [9];
    z ^= y;
    z  = z << 4 | z >> 28;
    x ^= * (u32 *) ((u8 *) (des_keymap + 448) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 384) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 320) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 256) + (0XFC & z));
    z  = r [10];
    z ^= x;
    y ^= * (u32 *) ((u8 *) (des_keymap + 192) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 128) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 64) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) des_keymap + (0XFC & z));
    z  = r [11];
    z ^= x;
    z  = z << 4 | z >> 28;
    y ^= * (u32 *) ((u8 *) (des_keymap + 448) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 384) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 320) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 256) + (0XFC & z));
    z  = r [12];
    z ^= y;
    x ^= * (u32 *) ((u8 *) (des_keymap + 192) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 128) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 64) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) des_keymap + (0XFC & z));
    z  = r [13];
    z ^= y;
    z  = z << 4 | z >> 28;
    x ^= * (u32 *) ((u8 *) (des_keymap + 448) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 384) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 320) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 256) + (0XFC & z));
    z  = r [14];
    z ^= x;
    y ^= * (u32 *) ((u8 *) (des_keymap + 192) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 128) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 64) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) des_keymap + (0XFC & z));
    z  = r [15];
    z ^= x;
    z  = z << 4 | z >> 28;
    y ^= * (u32 *) ((u8 *) (des_keymap + 448) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 384) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 320) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 256) + (0XFC & z));
    z  = r [16];
    z ^= y;
    x ^= * (u32 *) ((u8 *) (des_keymap + 192) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 128) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 64) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) des_keymap + (0XFC & z));
    z  = r [17];
    z ^= y;
    z  = z << 4 | z >> 28;
    x ^= * (u32 *) ((u8 *) (des_keymap + 448) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 384) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 320) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 256) + (0XFC & z));
    z  = r [18];
    z ^= x;
    y ^= * (u32 *) ((u8 *) (des_keymap + 192) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 128) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 64) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) des_keymap + (0XFC & z));
    z  = r [19];
    z ^= x;
    z  = z << 4 | z >> 28;
    y ^= * (u32 *) ((u8 *) (des_keymap + 448) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 384) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 320) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 256) + (0XFC & z));
    z  = r [20];
    z ^= y;
    x ^= * (u32 *) ((u8 *) (des_keymap + 192) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 128) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 64) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) des_keymap + (0XFC & z));
    z  = r [21];
    z ^= y;
    z  = z << 4 | z >> 28;
    x ^= * (u32 *) ((u8 *) (des_keymap + 448) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 384) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 320) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 256) + (0XFC & z));
    z  = r [22];
    z ^= x;
    y ^= * (u32 *) ((u8 *) (des_keymap + 192) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 128) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 64) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) des_keymap + (0XFC & z));
    z  = r [23];
    z ^= x;
    z  = z << 4 | z >> 28;
    y ^= * (u32 *) ((u8 *) (des_keymap + 448) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 384) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 320) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 256) + (0XFC & z));
    z  = r [24];
    z ^= y;
    x ^= * (u32 *) ((u8 *) (des_keymap + 192) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 128) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 64) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) des_keymap + (0XFC & z));
    z  = r [25];
    z ^= y;
    z  = z << 4 | z >> 28;
    x ^= * (u32 *) ((u8 *) (des_keymap + 448) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 384) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 320) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 256) + (0XFC & z));
    z  = r [26];
    z ^= x;
    y ^= * (u32 *) ((u8 *) (des_keymap + 192) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 128) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 64) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) des_keymap + (0XFC & z));
    z  = r [27];
    z ^= x;
    z  = z << 4 | z >> 28;
    y ^= * (u32 *) ((u8 *) (des_keymap + 448) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 384) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 320) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 256) + (0XFC & z));
    z  = r [28];
    z ^= y;
    x ^= * (u32 *) ((u8 *) (des_keymap + 192) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 128) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 64) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) des_keymap + (0XFC & z));
    z  = r [29];
    z ^= y;
    z  = z << 4 | z >> 28;
    x ^= * (u32 *) ((u8 *) (des_keymap + 448) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 384) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 320) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 256) + (0XFC & z));
    z  = r [30];
    z ^= x;
    y ^= * (u32 *) ((u8 *) (des_keymap + 192) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 128) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 64) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) des_keymap + (0XFC & z));
    z  = r [31];
    z ^= x;
    z  = z << 4 | z >> 28;
    y ^= * (u32 *) ((u8 *) (des_keymap + 448) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 384) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 320) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 256) + (0XFC & z));
    x  = x << 1 | x >> 31;
    z  = (x ^ y) & 0X55555555L;
    y ^= z;
    x ^= z;
    y  = y << 1 | y >> 31;
    z  = ((x >> 010) ^ y) & 0X00FF00FFL;
    x ^= z << 010;
    y ^= z;
    z  = ((y >> 002) ^ x) & 0X33333333L;
    y ^= z << 002;
    x ^= z;
    z  = ((x >> 020) ^ y) & 0X0000FFFFL;
    x ^= z << 020;
    y ^= z;
    z  = ((y >> 004) ^ x) & 0X0F0F0F0FL;
    y ^= z << 004;
    x ^= z;
    d [0] = x;
    x >>= 8;
    d [1] = x;
    x >>= 8;
    d [2] = x;
    x >>= 8;
    d [3] = x;
    d [4] = y;
    y >>= 8;
    d [5] = y;
    y >>= 8;
    d [6] = y;
    y >>= 8;
    d [7] = y;
    return;
}

static void DesSmallFipsDecrypt (u8 *d, u32 *r, u8 *s)
{
    register u32 x, y, z;
    x  = s [7];
    x <<= 8;
    x |= s [6];
    x <<= 8;
    x |= s [5];
    x <<= 8;
    x |= s [4];
    y  = s [3];
    y <<= 8;
    y |= s [2];
    y <<= 8;
    y |= s [1];
    y <<= 8;
    y |= s [0];
    z  = ((x >> 004) ^ y) & 0X0F0F0F0FL;
    x ^= z << 004;
    y ^= z;
    z  = ((y >> 020) ^ x) & 0X0000FFFFL;
    y ^= z << 020;
    x ^= z;
    z  = ((x >> 002) ^ y) & 0X33333333L;
    x ^= z << 002;
    y ^= z;
    z  = ((y >> 010) ^ x) & 0X00FF00FFL;
    y ^= z << 010;
    x ^= z;
    x  = x >> 1 | x << 31;
    z  = (x ^ y) & 0X55555555L;
    y ^= z;
    x ^= z;
    y  = y >> 1 | y << 31;
    z  = r [31];
    z ^= y;
    z  = z << 4 | z >> 28;
    x ^= * (u32 *) ((u8 *) (des_keymap + 448) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 384) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 320) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 256) + (0XFC & z));
    z  = r [30];
    z ^= y;
    x ^= * (u32 *) ((u8 *) (des_keymap + 192) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 128) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 64) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) des_keymap + (0XFC & z));
    z  = r [29];
    z ^= x;
    z  = z << 4 | z >> 28;
    y ^= * (u32 *) ((u8 *) (des_keymap + 448) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 384) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 320) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 256) + (0XFC & z));
    z  = r [28];
    z ^= x;
    y ^= * (u32 *) ((u8 *) (des_keymap + 192) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 128) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 64) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) des_keymap + (0XFC & z));
    z  = r [27];
    z ^= y;
    z  = z << 4 | z >> 28;
    x ^= * (u32 *) ((u8 *) (des_keymap + 448) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 384) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 320) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 256) + (0XFC & z));
    z  = r [26];
    z ^= y;
    x ^= * (u32 *) ((u8 *) (des_keymap + 192) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 128) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 64) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) des_keymap + (0XFC & z));
    z  = r [25];
    z ^= x;
    z  = z << 4 | z >> 28;
    y ^= * (u32 *) ((u8 *) (des_keymap + 448) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 384) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 320) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 256) + (0XFC & z));
    z  = r [24];
    z ^= x;
    y ^= * (u32 *) ((u8 *) (des_keymap + 192) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 128) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 64) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) des_keymap + (0XFC & z));
    z  = r [23];
    z ^= y;
    z  = z << 4 | z >> 28;
    x ^= * (u32 *) ((u8 *) (des_keymap + 448) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 384) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 320) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 256) + (0XFC & z));
    z  = r [22];
    z ^= y;
    x ^= * (u32 *) ((u8 *) (des_keymap + 192) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 128) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 64) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) des_keymap + (0XFC & z));
    z  = r [21];
    z ^= x;
    z  = z << 4 | z >> 28;
    y ^= * (u32 *) ((u8 *) (des_keymap + 448) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 384) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 320) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 256) + (0XFC & z));
    z  = r [20];
    z ^= x;
    y ^= * (u32 *) ((u8 *) (des_keymap + 192) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 128) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 64) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) des_keymap + (0XFC & z));
    z  = r [19];
    z ^= y;
    z  = z << 4 | z >> 28;
    x ^= * (u32 *) ((u8 *) (des_keymap + 448) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 384) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 320) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 256) + (0XFC & z));
    z  = r [18];
    z ^= y;
    x ^= * (u32 *) ((u8 *) (des_keymap + 192) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 128) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 64) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) des_keymap + (0XFC & z));
    z  = r [17];
    z ^= x;
    z  = z << 4 | z >> 28;
    y ^= * (u32 *) ((u8 *) (des_keymap + 448) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 384) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 320) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 256) + (0XFC & z));
    z  = r [16];
    z ^= x;
    y ^= * (u32 *) ((u8 *) (des_keymap + 192) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 128) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 64) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) des_keymap + (0XFC & z));
    z  = r [15];
    z ^= y;
    z  = z << 4 | z >> 28;
    x ^= * (u32 *) ((u8 *) (des_keymap + 448) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 384) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 320) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 256) + (0XFC & z));
    z  = r [14];
    z ^= y;
    x ^= * (u32 *) ((u8 *) (des_keymap + 192) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 128) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 64) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) des_keymap + (0XFC & z));
    z  = r [13];
    z ^= x;
    z  = z << 4 | z >> 28;
    y ^= * (u32 *) ((u8 *) (des_keymap + 448) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 384) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 320) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 256) + (0XFC & z));
    z  = r [12];
    z ^= x;
    y ^= * (u32 *) ((u8 *) (des_keymap + 192) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 128) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 64) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) des_keymap + (0XFC & z));
    z  = r [11];
    z ^= y;
    z  = z << 4 | z >> 28;
    x ^= * (u32 *) ((u8 *) (des_keymap + 448) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 384) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 320) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 256) + (0XFC & z));
    z  = r [10];
    z ^= y;
    x ^= * (u32 *) ((u8 *) (des_keymap + 192) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 128) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 64) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) des_keymap + (0XFC & z));
    z  = r [9];
    z ^= x;
    z  = z << 4 | z >> 28;
    y ^= * (u32 *) ((u8 *) (des_keymap + 448) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 384) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 320) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 256) + (0XFC & z));
    z  = r [8];
    z ^= x;
    y ^= * (u32 *) ((u8 *) (des_keymap + 192) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 128) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 64) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) des_keymap + (0XFC & z));
    z  = r [7];
    z ^= y;
    z  = z << 4 | z >> 28;
    x ^= * (u32 *) ((u8 *) (des_keymap + 448) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 384) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 320) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 256) + (0XFC & z));
    z  = r [6];
    z ^= y;
    x ^= * (u32 *) ((u8 *) (des_keymap + 192) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 128) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 64) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) des_keymap + (0XFC & z));
    z  = r [5];
    z ^= x;
    z  = z << 4 | z >> 28;
    y ^= * (u32 *) ((u8 *) (des_keymap + 448) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 384) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 320) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 256) + (0XFC & z));
    z  = r [4];
    z ^= x;
    y ^= * (u32 *) ((u8 *) (des_keymap + 192) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 128) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 64) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) des_keymap + (0XFC & z));
    z  = r [3];
    z ^= y;
    z  = z << 4 | z >> 28;
    x ^= * (u32 *) ((u8 *) (des_keymap + 448) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 384) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 320) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 256) + (0XFC & z));
    z  = r [2];
    z ^= y;
    x ^= * (u32 *) ((u8 *) (des_keymap + 192) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 128) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) (des_keymap + 64) + (0XFC & z));
    z >>= 8;
    x ^= * (u32 *) ((u8 *) des_keymap + (0XFC & z));
    z  = r [1];
    z ^= x;
    z  = z << 4 | z >> 28;
    y ^= * (u32 *) ((u8 *) (des_keymap + 448) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 384) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 320) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 256) + (0XFC & z));
    z  = r [0];
    z ^= x;
    y ^= * (u32 *) ((u8 *) (des_keymap + 192) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 128) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) (des_keymap + 64) + (0XFC & z));
    z >>= 8;
    y ^= * (u32 *) ((u8 *) des_keymap + (0XFC & z));
    x  = x << 1 | x >> 31;
    z  = (x ^ y) & 0X55555555L;
    y ^= z;
    x ^= z;
    y  = y << 1 | y >> 31;
    z  = ((x >> 010) ^ y) & 0X00FF00FFL;
    x ^= z << 010;
    y ^= z;
    z  = ((y >> 002) ^ x) & 0X33333333L;
    y ^= z << 002;
    x ^= z;
    z  = ((x >> 020) ^ y) & 0X0000FFFFL;
    x ^= z << 020;
    y ^= z;
    z  = ((y >> 004) ^ x) & 0X0F0F0F0FL;
    y ^= z << 004;
    x ^= z;
    d [0] = x;
    x >>= 8;
    d [1] = x;
    x >>= 8;
    d [2] = x;
    x >>= 8;
    d [3] = x;
    d [4] = y;
    y >>= 8;
    d [5] = y;
    y >>= 8;
    d [6] = y;
    y >>= 8;
    d [7] = y;
    return;
}


static int des_ede3_encrypt(struct cipher_context *cx,
			    const u8 *in, u8 *out, int size, int atomic)
{
	u8 input_buffer[8], trans_buffer1[8];
	u8 trans_buffer2[8], output_buffer[8];

	u32 *key_sched;

	key_sched = cx->keyinfo;

	for (; size >= 8; size -= 8) {
		memcpy(&input_buffer, in, 8);
		in += 8;
		DesSmallFipsEncrypt(trans_buffer1,key_sched,input_buffer);
		DesSmallFipsDecrypt(trans_buffer2,key_sched+32,trans_buffer1);
		DesSmallFipsEncrypt(output_buffer,key_sched+64,trans_buffer2);
		memcpy(out, &output_buffer, 8);
		out += 8;
	}
	return 0;
}

static int des_ede3_decrypt(struct cipher_context *cx,
			    const u8 *in, u8 *out, int size, int atomic)
{
  	u8 input_buffer[8], trans_buffer1[8];
	u8 trans_buffer2[8], output_buffer[8];

	u32 *key_sched;

	key_sched = cx->keyinfo;

	for (; size >= 8; size -= 8) {
		memcpy(&input_buffer, in, 8);
		in += 8;
		DesSmallFipsDecrypt(trans_buffer1,key_sched+64,input_buffer);
		DesSmallFipsEncrypt(trans_buffer2,key_sched+32,trans_buffer1);
		DesSmallFipsDecrypt(output_buffer,key_sched,trans_buffer2);		
		memcpy(out, &output_buffer, 8);
		out += 8;
	}
	return 0;
}


static void des_ede3_lock(void)
{
	MOD_INC_USE_COUNT;
}

static void des_ede3_unlock(void)
{
	MOD_DEC_USE_COUNT;
}   


#define CIPHER_BITS_64
#define CIPHER_NAME(x) des_ede3##x
#include "gen-cbc.h"
#include "gen-ecb.h"

#define DES_EDE3_KEY_SCHEDULE_SIZE (3*32*sizeof(u32))

static struct cipher_implementation des_ede3_ecb = {
	{{NULL,NULL}, CIPHER_MODE_ECB, "des_ede3-ecb"},
	blocksize: 8,
	ivsize: 0,
	key_schedule_size: DES_EDE3_KEY_SCHEDULE_SIZE,
	key_size_mask: CIPHER_KEYSIZE_64,
	INIT_CIPHER_BLKOPS(des_ede3_ecb),
	INIT_CIPHER_OPS(des_ede3)
};

static struct cipher_implementation des_ede3_cbc = {
	{{NULL,NULL}, CIPHER_MODE_CBC, "des_ede3-cbc"},
	blocksize: 8,
	ivsize: 8,
	key_schedule_size: DES_EDE3_KEY_SCHEDULE_SIZE,
	key_size_mask: CIPHER_KEYSIZE_64 | CIPHER_KEYSIZE_128 | 
	               CIPHER_KEYSIZE_192,
	INIT_CIPHER_BLKOPS(des_ede3_cbc),
	INIT_CIPHER_OPS(des_ede3)
};

static int __init init_des_ede3(void)
{
    if (register_cipher(&des_ede3_ecb))
        printk(KERN_WARNING "Couldn't register des_ede3-ecb encryption\n");
    if (register_cipher(&des_ede3_cbc))
        printk(KERN_WARNING "Couldn't register des_ede3-cbc encryption\n");

    return 0;
}

static void __exit cleanup_des_ede3(void)
{
	if (unregister_cipher(&des_ede3_ecb))
		printk(KERN_WARNING "Couldn't unregister des_ede3-ecb encryption\n");
	if (unregister_cipher(&des_ede3_cbc))
		printk(KERN_WARNING "Couldn't unregister des_ede3-cbc encryption\n");
}

module_init(init_des_ede3);
module_exit(cleanup_des_ede3);

MODULE_LICENSE("GPL");

EXPORT_NO_SYMBOLS;

/* eof */
