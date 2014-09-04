/*
 *  linux/kernel/ilatency.h
 *
 *  Interrupt latency instrumentation
 *
 *  Copyright (C) 2001 MontaVista Software Inc.
 *
 *  2000-03-15  Jun Sun's original interrupt latency patch
 *  2001-8-29   Made to work on multiple arch's by Dave Singleton
 */

extern void inthoff_logentry(unsigned);
extern void intr_ret_from_exception(void);
extern void interrupt_overhead_start(void);
extern void interrupt_overhead_stop(void);
extern unsigned ticks_per_usec;

#define	NUM_LOG_ENTRIES 12
#define ON 		1
#define OFF 		0
#define	BUCKETS		250 
#define BUCKET_SIZE	2
#define UPPER_LIMIT	BUCKETS * BUCKET_SIZE
#define LAST_BUCKET	BUCKETS - 1

typedef struct interrupt_latency_log {
	/* count interrupt and iret */
	int iret_count;

	/* the test name */
	const char * testName;

	/* flag to control logging */
	unsigned logging;   /* 0 - no logging; 1 - logging */

	/* panic flag - set to 1 if something is realy wrong */
	unsigned really_wrong;

	/* for synchro between start and end */
	unsigned sync;

	/* we only log interrupts within certain range */
	unsigned low_water;
	unsigned high_water;

	/* count the total number interrupts  and intrs in range*/
	unsigned total_ints;
	unsigned total_ints_in_range;


	/* error accounting */
	unsigned skip_sti;
	unsigned skip_cli;
	unsigned sti_error;
	unsigned cli_error;
	unsigned sti_break_error;
	unsigned restore_sti;
	unsigned restore_cli;

	struct {
		/* worst blocking time */
		int interrupts_off;
		const char * cli_file;
		unsigned cli_line;
		unsigned cli_ticks;
		
		const char *sti_file;
		unsigned sti_line;
		unsigned sti_ticks;
		int multiples;
	} log[NUM_LOG_ENTRIES];
} interrupt_latency_log_t;

