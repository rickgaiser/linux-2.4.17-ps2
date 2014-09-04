//	ffolat.h

//	Copyright (c) 1999-2002, FOCUS Enhancements, Inc.  All Rights Reserved.

//	This file defines static structs used to compute TV out settings.

#ifndef		__FFOLAT_H__
#define		__FFOLAT_H__


#define FFO7x5 0x80
#define FFO7x4 0x40

typedef struct _S_FFOLAT
{
	int v_total;
	unsigned short ffolat;
} S_FFOLAT;

typedef struct _S_FFOLAT_IVO
{
	int v_total;
	unsigned short ivo;
	unsigned short ffolat;
} S_FFOLAT_IVO;

// h_total=832, ivo=40, tv_width=858, tv_lines=525, vga_lines=480
#define SIZE6X4NTSC		70
static const S_FFOLAT ffo6x4ntsc[SIZE6X4NTSC+1] =
{
	{ 525, 0x40 },
	{ 529, 0x40 },
	{ 533, 0x40 },
	{ 537, 0x80 },
	{ 541, 0x40 },
	{ 545, 0x40 },
	{ 549, 0x40 },
	{ 553, 0x40 },
	{ 557, 0x58 },
	{ 561, 0x40 },
	{ 565, 0x40 },
	{ 569, 0x40 },
	{ 573, 0x48 },
	{ 577, 0x40 },
	{ 581, 0x40 },
	{ 585, 0x40 },
	{ 589, 0x40 },
	{ 593, 0x48 },
	{ 597, 0x40 },
	{ 601, 0x40 },
	{ 605, 0x40 },
	{ 609, 0x40 },
	{ 613, 0x5b },
	{ 617, 0x48 },
	{ 621, 0x60 },
	{ 625, 0x48 },
	{ 629, 0x48 },
	{ 633, 0x40 },
	{ 637, 0x5e },
	{ 641, 0x40 },
	{ 645, 0x50 },
	{ 649, 0x56 },
	{ 653, 0x58 },
	{ 657, 0x6c },
	{ 661, 0x40 },
	{ 665, 0x40 },
	{ 669, 0x40 },
	{ 673, 0x40 },
	{ 677, 0x40 },
	{ 681, 0x40 },
	{ 685, 0x40 },
	{ 689, 0x40 },
	{ 693, 0x40 },
	{ 697, 0x40 },
	{ 701, 0x40 },
	{ 705, 0x40 },
	{ 709, 0x40 },
	{ 713, 0x40 },
	{ 717, 0x40 },
	{ 721, 0x40 },
	{ 725, 0x40 },
	{ 729, 0x40 },
	{ 733, 0x40 },
	{ 737, 0x40 },
	{ 741, 0x40 },
	{ 745, 0x40 },
	{ 749, 0x40 },
	{ 753, 0x40 },
	{ 757, 0x40 },
	{ 761, 0x40 },
	{ 765, 0x40 },
	{ 769, 0x40 },
	{ 773, 0x40 },
	{ 777, 0x40 },
	{ 781, 0x40 },
	{ 785, 0x40 },
	{ 789, 0x40 },
	{ 793, 0x40 },
	{ 797, 0x30 },
	{ 801, 0x40 },
	{ -1, 0 }
};

#define SIZE6X4PAL		45
static const S_FFOLAT ffo6x4pal[SIZE6X4PAL+1] =
{
	{ 625, 0x60 },
	{ 629, 0x60 },
	{ 633, 0x60 },
	{ 637, 0x60 },
	{ 641, 0x50 },
	{ 645, 0x60 },
	{ 649, 0x60 },
	{ 653, 0x60 },
	{ 657, 0x60 },
	{ 661, 0x60 },
	{ 665, 0x60 },
	{ 669, 0x60 },
	{ 673, 0x60 },
	{ 677, 0x60 },
	{ 681, 0x60 },
	{ 685, 0x60 },
	{ 689, 0x60 },
	{ 693, 0x60 },
	{ 697, 0x60 },
	{ 701, 0x60 },
	{ 705, 0x60 },
	{ 709, 0x60 },
	{ 713, 0x60 },
	{ 717, 0x60 },
	{ 721, 0x60 },
	{ 725, 0x60 },
	{ 729, 0x60 },
	{ 733, 0x60 },
	{ 737, 0x60 },
	{ 741, 0x60 },
	{ 745, 0x60 },
	{ 749, 0x60 },
	{ 753, 0x60 },
	{ 757, 0x60 },
	{ 761, 0x60 },
	{ 765, 0x60 },
	{ 769, 0x60 },
	{ 773, 0x60 },
	{ 777, 0x60 },
	{ 781, 0x60 },
	{ 785, 0x60 },
	{ 789, 0x60 },
	{ 793, 0x60 },
	{ 797, 0x60 },
	{ 801, 0x60 },
	{ -1, 0 }
};

// h_total=1056, vga_lines=600
#define	SIZE8X6NTSC		37
static const S_FFOLAT ffo8x6ntsc[SIZE8X6NTSC+1] = {
	{ 620, 0x50 },	// v_total_min >= vsync+10 >= vga_lines+10 = 610
	{ 625, 0x58 },
	{ 630, 0x40 },
	{ 635, 0x40 },
	{ 640, 0x40 },
	{ 645, 0x46 },
	{ 650, 0x46 },
	{ 655, 0x4f },
	{ 660, 0x4c },
	{ 665, 0x4a },
	{ 670, 0x50 },
	{ 675, 0x2f },
	{ 680, 0x48 },
	{ 685, 0x40 },
	{ 690, 0x31 },
	{ 695, 0x40 },
	{ 700, 0x21 },
	{ 705, 0x25 },
	{ 710, 0x40 },
	{ 715, 0x48 },
	{ 720, 0x50 },
	{ 725, 0x30 },
	{ 730, 0x50 },
	{ 735, 0x50 },
	{ 740, 0x50 },
	{ 745, 0x40 },
	{ 750, 0x50 },
	{ 755, 0x50 },
	{ 760, 0x50 },
	{ 765, 0x40 },
	{ 770, 0x50 },
	{ 775, 0x40 },
	{ 780, 0x40 },
	{ 785, 0x40 },
	{ 790, 0x50 },
	{ 795, 0x50 },
	{ 800, 0x50 },
	{ -1, 0 }
};

// h_total=1056, vga_lines=600
#define	SIZE8X6PAL		36
static const S_FFOLAT ffo8x6pal[SIZE8X6PAL+1] = {
	{ 625, 0x80 },
	{ 630, 0x80 },
	{ 635, 0x5a },
	{ 640, 0x55 },
	{ 645, 0x48 },
	{ 650, 0x65 },
	{ 655, 0x65 },
	{ 660, 0x50 },
	{ 665, 0x80 },
	{ 670, 0x70 },
	{ 675, 0x56 },
	{ 680, 0x80 },
	{ 685, 0x58 },
	{ 690, 0x31 },
	{ 695, 0x80 },
	{ 700, 0x60 },
	{ 705, 0x45 },
	{ 710, 0x4a },
	{ 715, 0x50 },
	{ 720, 0x50 },
	{ 725, 0x50 },
	{ 730, 0x45 },
	{ 735, 0x50 },
	{ 740, 0x50 },
	{ 745, 0x50 },
	{ 750, 0x50 },
	{ 755, 0x50 },
	{ 760, 0x50 },
	{ 765, 0x50 },
	{ 770, 0x50 },
	{ 775, 0x50 },
	{ 780, 0x50 },
	{ 785, 0x50 },
	{ 790, 0x50 },
	{ 795, 0x50 },
	{ 800, 0x50 },
	{ -1, 0 }
};

// h_total=1344, vga_lines=768
#define	SIZE10X7NTSC		45
static const S_FFOLAT_IVO ffo10x7ntsc[SIZE10X7NTSC] = {
	{ 783, 0x4d, 0x40 },
	{ 789, 0x47, 0x14 },
	{ 795, 0x47, 0x7f },
	{ 801, 0x47, 0x53 },
	{ 807, 0x47, 0x11 },
	{ 813, 0x47, 0x78 },
	{ 819, 0x47, 0x54 },
	{ 825, 0x47, 0x40 },
	{ 831, 0x47, 0x0f },
	{ 837, 0x4d, 0x40 },
	{ 843, 0x47, 0x5a },
	{ 849, 0x4d, 0x40 },
	{ 855, 0x47, 0x4b },
	{ 861, 0x4d, 0x40 },
	{ 867, 0x47, 0x4b },
	{ 873, 0x4d, 0x40 },
	{ 879, 0x47, 0x07 },
	{ 885, 0x48, 0x20 },
	{ 891, 0x47, 0x82 },
	{ 897, 0x47, 0x60 },
	{ 903, 0x47, 0x7f },
	{ 909, 0x4d, 0x40 },
	{ 915, 0x48, 0x40 },
	{ 921, 0x4c, 0x40 },
	{ 927, 0x49, 0x40 },
	{ 933, 0x48, 0x40 },
	{ 939, 0x4a, 0x40 },
	{ 945, 0x46, 0x40 },
	{ 951, 0x4a, 0x40 },
	{ 957, 0x4a, 0x40 },
	{ 963, 0x4b, 0x40 },
	{ 969, 0x4b, 0x40 },
	{ 975, 0x48, 0x40 },
	{ 981, 0x47, 0x40 },
	{ 987, 0x47, 0x40 },
	{ 993, 0x47, 0x40 },
	{ 999, 0x48, 0x40 },
	{ 1005, 0x48, 0x40 },
	{ 1011, 0x47, 0x40 },
	{ 1017, 0x47, 0x40 },
	{ 1023, 0x48, 0x40 },
	{ 1029, 0x48, 0x40 },
	{ 1035, 0x46, 0x40 },
	{ 1041, 0x47, 0x40 },
	{ 1047, 0x47, 0x40 }
};

// h_total=1344, vga_lines=768
#define	SIZE10X7PAL		46
static const S_FFOLAT_IVO ffo10x7pal[SIZE10X7PAL] = {
	{ 781, 0x49, 0x40 },
	{ 787, 0x46, 0x40 },
	{ 793, 0x48, 0x40 },
	{ 799, 0x46, 0x40 },
	{ 805, 0x49, 0x40 },
	{ 811, 0x47, 0x40 },
	{ 817, 0x46, 0x40 },
	{ 823, 0x46, 0x56 },
	{ 829, 0x46, 0x2d },
	{ 835, 0x46, 0x40 },
	{ 841, 0x46, 0x2d },
	{ 847, 0x46, 0x3f },
	{ 853, 0x46, 0x10 },
	{ 859, 0x46, 0x86 },
	{ 865, 0x46, 0xc9 },
	{ 871, 0x46, 0x83 },
	{ 877, 0x46, 0xa8 },
	{ 883, 0x46, 0x81 },
	{ 889, 0x46, 0xa5 },
	{ 895, 0x46, 0xa9 },
	{ 901, 0x46, 0x81 },
	{ 907, 0x46, 0xa4 },
	{ 913, 0x46, 0xa5 },
	{ 919, 0x46, 0x7f },
	{ 925, 0x46, 0xa2 },
	{ 931, 0x46, 0x9d },
	{ 937, 0x46, 0xc1 },
	{ 943, 0x46, 0x96 },
	{ 949, 0x46, 0xb7 },
	{ 955, 0x46, 0xb1 },
	{ 961, 0x46, 0x8a },
	{ 967, 0x46, 0xa9 },
	{ 973, 0x46, 0xa0 },
	{ 979, 0x46, 0x40 },
	{ 985, 0x46, 0x97 },
	{ 991, 0x46, 0xb5 },
	{ 997, 0x46, 0xaa },
	{ 1003, 0x46, 0x83 },
	{ 1009, 0x46, 0x9f },
	{ 1015, 0x47, 0x40 },
	{ 1021, 0x46, 0xad },
	{ 1027, 0x46, 0x87 },
	{ 1033, 0x46, 0xa2 },
	{ 1039, 0x47, 0x40 },
	{ 1045, 0x46, 0xac },
	{ 1051, 0x46, 0x86 }
};


#endif
