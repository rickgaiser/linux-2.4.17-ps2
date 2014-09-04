/* $XFree86: $ */


#ifndef MQ_H
#define MQ_H

#include "compiler.h"
#include "xaa.h"
#include "xf86Cursor.h"
#include "colormapst.h"

/*
typedef struct
{
    Bool	isHwCursor;
    int		CursorMaxWidth;
    int 	CursorMaxHeight;
    int		CursorFlags;
    long 	CursorStartAddr;
    Bool	(*UseHWCursor)(ScreenPtr, CursorPtr);
    void	(*LoadCursorImage)(ScrnInfoPtr, unsigned char*);
    void	(*ShowCursor)(ScrnInfoPtr);
    void	(*HideCursor)(ScrnInfoPtr);
    void	(*SetCursorPosition)(ScrnInfoPtr, int, int);
    void	(*SetCursorColors)(ScrnInfoPtr, int, int);
    void	(*LoadPalette)(ScrnInfoPtr, int, int*, LOCO*, VisualPtr);
    void	(*PreInit)(ScrnInfoPtr);
    void	(*Save)(ScrnInfoPtr, Bool);
    void	(*Restore)(ScrnInfoPtr, Bool);
    Bool	(*ModeInit)(ScrnInfoPtr, DisplayModePtr);
} MQRamdacRec, *MQRamdacPtr;
*/


typedef struct
{
    int bitsPerPixel;
    int depth;
    int displayWidth;
    rgb weight;
    DisplayModePtr mode;
} MQFBLayout;


/* Card-specific driver information */

#define MQPTR(p) ((MQPtr)((p)->driverPrivate))

typedef struct {
    EntityInfoPtr	pEnt;
    pciVideoPtr	        PciInfo;
    PCITAG		PciTag;
    //xf86AccessRec	Access;
    int			Chipset;
    int                 ChipRev;
    Bool		Primary;
    Bool		Interleave;
    int			HwBpp;
    CARD32		IOAddress;
    CARD32		FbAddress;
    unsigned char *     IOBase;
    unsigned char *     FbBase;
    unsigned char *     FbStart;
    long		FbMapSize;
    long		FbUsableSize;
    //long		FbCursorOffset;
    //MQRamdacRec	        Dac;
    Bool		NoAccel;
    Bool		HWCursor;
    Bool		ShadowFB;
    unsigned char *     ShadowPtr;
    int			ShadowPitch;
    int			BppShift;
    int			BppBitMask;
    int			Need64BitDummy;
    CARD32		DrawCommand;
    CARD32		FullPlaneMask;
    CARD32		AccelFlags;
    XAAInfoRecPtr	AccelInfoRec;
    xf86CursorInfoPtr	CursorInfoRec;
    //void		(*PreInit)(ScrnInfoPtr pScrn);
    void                (*Save)(ScrnInfoPtr, Bool);
    void                (*Restore)(ScrnInfoPtr, Bool);
    //Bool                (*ModeInit)(ScrnInfoPtr, DisplayModePtr);
    void                (*PointerMoved)(int index, int x, int y);
    CloseScreenProcPtr	CloseScreen;
    //ScreenBlockHandlerProcPtr BlockHandler;
    Bool                FBDev;
    int                 Rotate;
    MQFBLayout          CurrentLayout;
    int			MQFlag;
    unsigned long	*SrcImageDataAddr;
    //HSU: DGA Specific stuff
    DGAModePtr          DGAModes;
    int                 numDGAModes;
    int			DGAactive;
    int                 DGAViewportStatus;
    Bool		DrawTransparent;
    //HSU: Debugging use
    //CARD32		wh, skipneww, pMQ->misc;
} MQRec, *MQPtr;

#define CLIPPER_ON		0x00000002

#define MQ_FRONT	0
#define MQ_BACK	        1
#define MQ_DEPTH	2


#endif
