/***************************************************************************
 MQPROTO.H

 MQ200 common function prototypes

 Copyright (c) 2000 by MediaQ, Incorporated.
 All Rights Reserved.

***************************************************************************/
#ifndef MQPROTO_H
#define MQPROTO_H

/*
 * mqdata.h
 */
extern FPDATA_CONTROL fpControlData[];
extern DISPLAY_TIMING TimingParam[];

/*
 * mq_driver.c
 */
void MQAdjustFrame(int scrnIndex, int x, int y, int flags);
Bool MQSwitchMode(int scrnIndex, DisplayModePtr mode, int flags);

/*
 * mq_utils.c routines
 */
unsigned long idmqchip(void *pMQMMIO);
void setmqmode(PDISPLAY_CONFIG pDC, MQPtr pMQ);
void setupfp(int panel, void *pMQMMIO);
unsigned long computehfbuffer(PFPDATA_CONTROL pFP);
void setuphfbuffer(int panel, unsigned long sizeused,
		unsigned long *usable, void *pMQMMOI);
void setupgc(int gc, int x, int y, int bpp, int refresh, void *pMQMMIO);
void setupgcmem(PDISPLAY_CONFIG pDC, unsigned long startaddr, void *pMQMMIO);
void setpal(int index, unsigned long color, void *pMQMMIO);
void invblank(void *pMQMMIO);
#ifdef DUMP_MQ200_REG
void dump_mq200(void *pMQMMIO);
#endif
//void setpmmisc(PDISPLAY_CONFIG pDC, void *pMQMMIO);
PDISPLAY_TIMING getgcparam(int x, int y, int refresh);
unsigned long getbppbits(int bpp);
void onoffdisplay(int display_flag, void *pMQMMIO);
void setup_cursor(unsigned long cursorstart, void *pMQMMIO);
BOOL MQGetOptValInteger(OptionInfoPtr table, int token, int *val);
BOOL MQGetOptValULONG(OptionInfoPtr table, int token, unsigned long *val);

/*
 * mq_shadow.c routines
 */
void MQRefreshArea(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
void MQRefreshArea8(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
void MQRefreshArea16(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
void MQRefreshArea24(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
void MQRefreshArea32(ScrnInfoPtr pScrn, int num, BoxPtr pbox);
void MQPointerMoved(int index, int x, int y);

/*
 * mq_cursor.c routines
 */
Bool MQHWCursorInit(ScreenPtr pScreen);
void MQSetupFuncs(ScrnInfoPtr pScrn);
void MQLoadPalette( ScrnInfoPtr pScrn, int numColors, int *indices,
			LOCO *colors, VisualPtr pVisual);
//void move_cursor(unsigned long pos, unsigned long addr, void *pMQMMIO);

/*
 * mq_accel.c routines
 */
Bool MQAccelInit(ScreenPtr pScreen);

/*
 * mq_ge.c routines
 */
void MQSync(ScrnInfoPtr pScrn);

#endif /* MQPROTO_H */
