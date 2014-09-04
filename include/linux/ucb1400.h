#ifndef _AC97_UCB1400_H_
#define _AC97_UCB1400_H_

/* Codec/vendor specific register defines for the Philips UCB1400 AC link
*  codec.  Most all 2.0 functionality is supported.  The 1400 includes
*  support for 10 gpio lines plus a 4-wire panel touch screen controller.
*/

#define UCB1400_IO_DATA		0x5a	/* bits 0-9 valid */
#define UCB1400_IO_DIR		0x5c	/* DDR register for the above bits */
#define UCB1400_POS_INT_EN	0x5e
#define UCB1400_NEG_INT_EN	0x60
#define UCB1400_INT_CLR_STAT	0x62
#define UCB1400_TS_CTL		0x64
#define UCB1400_ADC_CTL		0x66
#define UCB1400_ADC_DATA	0x68
#define UCB1400_FEATURE_CSR1	0x6a
#define UCB1400_FEATURE_CSR2	0x6c
#define UCB1400_TEST_CTL	0x6e

#endif  //_AC97_UCB1400_H_
