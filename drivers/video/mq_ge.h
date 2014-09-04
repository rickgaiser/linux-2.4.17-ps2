
#ifndef _MQ_GE_H
#define _MQ_GE_H

#ifdef CHECK_CMDFIFO
#define geWAITCMDFIFO(cnt)   WaitCmdFIFO( pMQ, cnt )
#else
#define geWAITCMDFIFO(cnt)
#endif

#ifdef CHECK_SRCFIFO
#define geWAITSRCFIFO(cnt)   WaitSrcFIFO( pMQ, cnt )
#else
#define geWAITSRCFIFO(cnt)
#endif

#ifdef CHECK_NOTBUSY
#define geWAITNOTBUSY        WaitNotBusy( pMQ )
#else
#define geWAITNOTBUSY
#endif

#ifdef MQ_PCI
#define	geREG_2( idx1, val1, idx2, val2 ) \
	geREG( idx2, val2 ); \
	geREG( idx1, val1 )
#define	geREG_3( idx1, val1, idx2, val2, idx3, val3 ) \
	geREG_2( idx2, val2, idx3, val3 ); \
	geREG( idx1, val1 )
#define	geREG_4( idx1, val1, idx2, val2, idx3, val3, idx4, val4 ) \
	geREG_3( idx2, val2, idx3, val3, idx4, val4 ); \
	geREG( idx1, val1 )
#define	geREG_5( idx1, val1, idx2, val2, idx3, val3, idx4, val4, idx5, val5 ) \
	geREG_4( idx2, val2, idx3, val3, idx4, val4, idx5, val5 ); \
	geREG( idx1, val1 )
#else	//MQ_PCI
#define	geREG_2( idx1, val1, idx2, val2 ) \
	geREG( idx1, val1 ); \
	geREG( idx2, val2 )
#define	geREG_3( idx1, val1, idx2, val2, idx3, val3 ) \
	geREG( idx1, val1 ); \
	geREG_2( idx2, val2, idx3, val3 )
#define	geREG_4( idx1, val1, idx2, val2, idx3, val3, idx4, val4 ) \
	geREG( idx1, val1 ); \
	geREG_3( idx2, val2, idx3, val3, idx4, val4 )
#define	geREG_5( idx1, val1, idx2, val2, idx3, val3, idx4, val4, idx5, val5 ) \
	geREG( idx1, val1 ); \
	geREG_4( idx2, val2, idx3, val3, idx4, val4, idx5, val5 )
#endif	//MQ_PCI

void MQSync(ScrnInfoPtr pScrn);
void MQSetupForScreenToScreenCopy( ScrnInfoPtr pScrn, int xdir, int ydir,
                                int rop, unsigned int planemask, int trans);
void MQSubsequentScreenToScreenCopy(ScrnInfoPtr pScrn,
				int srcX, int srcY, int dstX, int dstY,
				int w, int h);
void MQSetupForCPUToScreenColorExpandFill(ScrnInfoPtr pScrn, int fg,
				int bg, int rop, unsigned int planemask);
void MQSubsequentCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
				int x, int y, int w, int h, int skipleft);
void MQSetupForSolidFill( ScrnInfoPtr pScrn, int color, int rop,
	                        unsigned int planemask );
void MQSubsequentSolidFillRect(ScrnInfoPtr pScrn,
			        int x, int y, int w, int h);
void MQSubsequentSolidHorVertLine(ScrnInfoPtr pScrn,
                                int x, int y, int len, int dir);
void MQSubsequentSolidHorVertLine(ScrnInfoPtr pScrn,
                                int x, int y, int len, int dir);
void MQSubsequentSolidBresenhamLine( ScrnInfoPtr pScrn,
	                        int x, int y, int absmaj, int absmin,
                                int err, int len, int octant);
void MQSetupForImageWrite(ScrnInfoPtr pScrn, int rop,
   				unsigned int planemask,
				int transparency_color, int bpp, int depth);
void MQSubsequentImageWrite(ScrnInfoPtr pScrn,
				int x, int y, int w, int h, int skipleft);
void MQSetupForMono8x8PatternFill(ScrnInfoPtr pScrn,
				int patx, int paty, int fg, int bg,
				int rop, unsigned int planemask);
void MQSubsequentMono8x8PatternFillRect(ScrnInfoPtr pScrn,
				int patx, int paty,
				int x, int y, int w, int h );
void MQSetupForScreenToScreenColorExpandFill(ScrnInfoPtr pScrn,
				int fg, int bg, int rop, 
				unsigned int planemask);
void MQSubsequentScreenToScreenColorExpandFill(ScrnInfoPtr pScrn,
				int x, int y, int w, int h,
				int srcx, int srcy, int skipleft);
void MQSetClippingRectangle(ScrnInfoPtr pScrn, int x1, int y1,
				int x2, int y2);
void MQDisableClipping(ScrnInfoPtr pScrn);
/*
void MQVPolylinesThinSolidWrapper(DrawablePtr, GCPtr, int, int, 
				DDXPointPtr);
void MQVPolySegmentThinSolidWrapper(DrawablePtr, GCPtr, int, xSegment*);
*/
/*
void MQSetupForDashedLine(ScrnInfoPtr pScrn, int fg, int bg, 
				int rop, unsigned int planemask, int length,
    				unsigned char *pattern);
void MQSubsequentDashedTwoPointLine(ScrnInfoPtr pScrn,
				int x1, int y1, int x2, int y2, 
				int flags, int phase);
*/

/*
#ifdef XF86DRI
void MQDRIInitBuffers(WindowPtr pWin, 
			     RegionPtr prgn, CARD32 index);
void MQDRIMoveBuffers(WindowPtr pParent, DDXPointRec ptOldOrg,
			     RegionPtr prgnSrc, CARD32 index);

#endif
*/

#if 0
extern void MQWriteBitmapColorExpand(ScrnInfoPtr pScrn, int x, int y,
				int w, int h, unsigned char *src, int srcwidth,
				int skipleft, int fg, int bg, int rop,
				unsigned int planemask);
extern void MQFillColorExpandRects(ScrnInfoPtr pScrn, int fg, int bg, int rop,
				unsigned int planemask, int nBox, BoxPtr pBox,
				int xorg, int yorg, PixmapPtr pPix);
extern void MQNonTEGlyphRenderer(ScrnInfoPtr pScrn, int x, int y, int n,
				NonTEGlyphPtr glyphs, BoxPtr pbox,
				int fg, int rop, unsigned int planemask);
extern void MQValidatePolyArc(GCPtr, unsigned long, DrawablePtr);
extern void MQValidatePolyPoint(GCPtr, unsigned long, DrawablePtr);
extern void MQFillCacheBltRects(ScrnInfoPtr, int, unsigned int, int, BoxPtr,
				int, int, XAACacheInfoPtr);
extern void MQTEGlyphRenderer(ScrnInfoPtr, int, int, int, int, int, int, 
				unsigned int **glyphs, int, int, int, int, 
				unsigned planemask);
#endif

#endif  //_MQ_GE_H
