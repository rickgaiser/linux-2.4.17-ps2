/*
 * Preemption Latency Measurement Tool
 * Copyright (C) 2001 MontaVista Software Inc.
 *
 * Measures the duration of locks held in the kernel.
 * Useful for pinpointing long-held locks, which we
 * can not preempt during.
 *
 * * 20011220 Robert M. Love
 * 	- add SH arch support
 * 	- tidy things to better support new arches
 * 	- add include/asm/preem_latency.h
 * 	- now synced with 2.4.17
 * 
 * * 20011115 Robert M. Love
 * 	- resync with 2.4.15-pre4 and the latest preempt
 * 	  kernel patch
 * 
 * * 20010923 Robert M. Love
 * 	- remove causes "lowlat" and "check"
 * 	- update for revised preemption patch
 * 	- remove defines for preempt_lock_check and window
 * 
 * * 20010919 Robert M. Love
 * 	whitespace cleanup, remove unneeded code, remove
 * 	ifdefs around latency_cause, etc
 * 
 * * 20010918 Robert M. Love
 * 	update for 2.4.10-pre11 and 2.4.9-ac13
 */

#include <asm/system.h>
#include <asm/current.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/interrupt.h>

/*
 * we need a method of timing.  On i386 this is easy
 * with the TSC.  On other arches we can use their
 * existing timing systems or hack something together.
 *
 * The defines are:
 * 	readclock(x)      put timing value in x (unsigned int)
 * 	readclock_init()  initialize timing
 *
 * readclock must be a monotonic incremental counter.
 */ 
#include <asm/preem_latency.h>

#define NUM_LOG_ENTRY 20
#define NUM_COUNT 2
#define COMMAND_LENGTH 16
#define NUM_INTERRUPT 16

const char * syscallStartFileName = "system_call";
const char * entryCheckFileName = "ret_with_reschedule";
const char * errorCodeStart = "error_code start";
const char * deviceNotAvailableStart = "device_not_availablestart";

extern void latency_check(const char *, unsigned, int);
extern void latency_end(const char *, unsigned);

unsigned theSyscallNumber;

#define SfS 0 /* waiting for a preempt off start1 call */
#define SfC 1 /* waiting for a preempt off end call */
#define SfM 2 /* waiting for either of the above */

struct pointId {
	const char * FileName;
	unsigned Address;    
	unsigned FileLine;
	unsigned ProcId;
	char ProcName[COMMAND_LENGTH];
};

/* worst recorded latencies */
struct log_entry {
	unsigned latency;
	int cause;
	int mask;
	unsigned intrCount[NUM_INTERRUPT+1];
	struct pointId start;
	struct pointId end;
};

static unsigned logFlag = 0;	/* 0 - no logging; 1 - logging */
static unsigned panicFlag = 0;	/* set to 1 if something is really wrong */

struct LatencyData {
	int breakCount;		/* count interrupt and iret */
	const char * testName;	/* the test name */
	unsigned syncFlag;	/* for synchro between start and end */
	unsigned numIntrs;	/* total number interrupts & intrs in range*/
	unsigned numCount;	/* layout */
	unsigned numEntry;
	int least;		/* lowest latency we care about */
	unsigned count[NUM_COUNT];
	struct log_entry  entry[NUM_LOG_ENTRY]; /* worst recorded latencies */
	int theCause;
	int mask;
	struct pointId latencyStart;
	unsigned latencyStartCount;
	unsigned startFlag;	/* to debug between start() and start1() */

};

struct LatencyData latencyData[NR_CPUS] = { {0} };

static char *cause_name[] = {
	"Unknown",
	"spin_lock",
	"reacqBKL",
	"BKL"
};

#define CAUSE_MAX (sizeof(cause_name)/sizeof(cause_name[0]) - 1)

#ifdef CLOCK_COUNTS_DOWN
#define CLKDIFF(start,end) ((start) - (end))
#else
#define CLKDIFF(start,end) ((end) - (start))
#endif

unsigned numIntr = NUM_INTERRUPT;
unsigned intrCount[NUM_INTERRUPT+1];

#define CLEAR(x) do { \
			int zz; \
			for (zz=0; zz<= NUM_INTERRUPT; zz++) \
			x[zz] = 0; \
		} while(0)

#define COPY(x, y) do { \
			int zz; \
			for (zz=0; zz<= NUM_INTERRUPT; zz++) \
			x[zz] = y[zz]; \
		} while(0)

/* strategy : 
 * 
 * Four points are identified
 *   S : the starting point right after system call, user -> kernel
 *   S1: alternative starting points 
 *      . faults (including device_not_available which does go through 
 *              error_code : label)
 *      . interrupts (all go through do_IRQ)
 *   C : need_resched is checked and schedule() is invoked if flag is set
 *   B : before RESTORE ALL and if control is returnning to user land
 *
 * For S points :
 *      it MUST follow a break point
 *      it initializes timing values for the starting point
 *
 * For S1 points :
 *      if it follows a breakpoint, treat it as an S point
 *      otherwise, ignore it.
 *
 * For C points :
 *      it must follow a C point or a S point
 *      calculate the interval and reset the starting point
 *
 * For B points :
 *      It must follow a C point or an S point
 *      calculate the internval and invalidate the starting point
 *
 */

static void reset_latencyData(void)
{
	int i;
	int cpu;

	for (cpu = 0; cpu < smp_num_cpus; cpu++) {
		latencyData[cpu].numCount = NUM_COUNT;
		latencyData[cpu].numEntry = NUM_LOG_ENTRY;
		latencyData[cpu].startFlag = 0;
		latencyData[cpu].breakCount = 0;
		latencyData[cpu].syncFlag = SfS;
		latencyData[cpu].numIntrs = 0;
		latencyData[cpu].least = 0;
		for (i = 0; i < latencyData[cpu].numCount; 
		     latencyData[cpu].count[i++] = 0);
	}
	return;
}

asmlinkage void latency_start(const char *fname,unsigned lineno, int cause)
{
	struct LatencyData *l = &latencyData[smp_processor_id()];

	/* if we are not logging or we have an error, do nothing */
	if ((logFlag == 0) || (panicFlag != 0))
		return;

	if (l->syncFlag != SfC) {
		readclock(l->latencyStartCount);
		l->startFlag = 1;
		l->mask = 0;
		l->theCause = cause;
		l->syncFlag = SfC;
		l->latencyStart.Address = (int)__builtin_return_address(0);
		l->latencyStart.FileName = fname;
		l->latencyStart.FileLine = lineno;
		l->latencyStart.ProcId = current->pid;
		memcpy(l->latencyStart.ProcName, current->comm, 16);
		CLEAR(intrCount);
        }
}

void latency_cause(int was, int tobe)
{
	struct LatencyData *l = &latencyData[smp_processor_id()];

	if (was == -100) {
		l->mask |= tobe;
		return;
        }

	if (l->theCause == was)
		l->theCause = tobe;
}

void latency_logentry(unsigned diff, const char *fname,
		unsigned lineno, unsigned Address, int mask)
{
	struct LatencyData *l = &latencyData[smp_processor_id()];
	unsigned lowest = 0xffffffff;
	unsigned lowestIndex = 0;
	unsigned sameIndex = 0xffffffff;
	int i;

	if (diff < l->least) 
		return;

	/* check if we need to log this event */
	for (i = 0; i < NUM_LOG_ENTRY; i++) {
		if (lowest > l->entry[i].latency) {
			lowest = l->entry[i].latency;
			lowestIndex = i;
		}
		/* If the call addresses match, it is the same path */
		if ((Address == l->entry[i].end.Address) &&
		 (l->latencyStart.Address == l->entry[i].start.Address)){
			sameIndex = i;
			break;
		}
	}

	if (sameIndex == 0xffffffff)  {
		i = lowestIndex;
		/*
		 * we would like "least" to be the smallest latency in the table
		 * but we are pressed for time so we settle for it being what 
		 * may have been the lowest, as this new entry may replace 
		 * the found lowest with a higher value, in which case we don't 
		 * have the true lowest.  Oh well, we  get closer as time 
		 * passes.  The real reason for this is to speed things up, 
		 * so we don't worry too much if this isn't exact.
		 */
		l->least = l->entry[i].latency;
	} else {
		i = sameIndex;
	}

	if (diff > l->entry[i].latency) {
		l->entry[i].latency = diff;
		l->entry[i].cause = l->theCause; 
		l->entry[i].mask = l->mask; 

		l->entry[i].end.FileName = fname;
		l->entry[i].end.FileLine = lineno;
		l->entry[i].end.Address = Address;
		l->entry[i].end.ProcId = current->pid;
		memcpy(l->entry[i].end.ProcName,
			current->comm, COMMAND_LENGTH);

		l->entry[i].start.FileName = l->latencyStart.FileName;
		l->entry[i].start.FileLine = l->latencyStart.FileLine;
		l->entry[i].start.Address = l->latencyStart.Address; 
		l->entry[i].start.ProcId = l->latencyStart.ProcId;
		memcpy(l->entry[i].start.ProcName, 
			l->latencyStart.ProcName, COMMAND_LENGTH);
	}

	l->numIntrs++;
}

/* Called at end of preemption time */
asmlinkage void latency_end(const char *fname, unsigned lineno)
{
	struct LatencyData *l = &latencyData[smp_processor_id()];
	unsigned endCount;

	/* if we are not logging or we have an error, do nothing */
	if ((logFlag == 0) || (panicFlag != 0)) {
		return;
	}

	/* read entry again */
	readclock(endCount);

	if (l->syncFlag == SfS) {
		l->count[SfS] ++;
		return;
	}

	latency_logentry(CLKDIFF(l->latencyStartCount, endCount), fname,
			 lineno, (int)__builtin_return_address(0), l->mask);
	l->syncFlag = SfS;
	return;
}

/* latency_check is for the end of one period and the start of another */
asmlinkage void latency_check(const char *fname, unsigned lineno, int cause)
{
	struct LatencyData *l = &latencyData[smp_processor_id()];
	unsigned endCount;

	/* if we are not logging or we have an error, do nothing */
	if ((logFlag == 0) || (panicFlag != 0)) {
		return;
	}
	/* read entry again */
	readclock(endCount);

	if (l->syncFlag != SfS) {
		latency_logentry(CLKDIFF(l->latencyStartCount, endCount), fname,
			     lineno, (int)__builtin_return_address(0), l->mask);
	}

	/* re-set the starting point */
	l->syncFlag = SfM;
	l->mask = 0;
	l->theCause = cause;
	l->latencyStart.Address = (int) __builtin_return_address(0);
	l->latencyStart.FileName = fname;
	l->latencyStart.FileLine = lineno;
	l->latencyStart.ProcId = current->pid;
	memcpy(l->latencyStart.ProcName, current->comm, 16);
	CLEAR(intrCount);
	readclock(l->latencyStartCount);
}

/* Note following code to read /proc file is not SMP-safe. */

static int g_read_completed = 0;
static int g_cpu = 0;

static int latencytimes_read_proc(char *page_buffer, char **my_first_byte,
	off_t virtual_start, int length, int *eof, void *data)
{
	int my_buffer_offset = 0;
	char * const my_base = page_buffer;
	int i,j,max;
	struct LatencyData *l;

	if (virtual_start == 0){
	/* Just been opened */
		logFlag = 0;  /* stop logging */
		g_read_completed = 0;
		g_cpu = 0;
	} else if (g_read_completed == NUM_LOG_ENTRY) {
		if (++g_cpu >= smp_num_cpus) {
			*eof = 1;
			reset_latencyData();
			logFlag = 1;  /* start logging */
			return 0;
		}

		g_read_completed = 0;
	}

	l = &latencyData[g_cpu];

	if (g_read_completed == 0) {
		my_buffer_offset += sprintf(my_base + my_buffer_offset, 
		 "cpu %d worst %d latency times of %d measured in this period.\n"
		 "  usec      cause     mask   start line/file      address   end line/file\n", g_cpu,NUM_LOG_ENTRY,l->numIntrs);
	}

	g_read_completed++;
	for (j = max = i = 0;j < NUM_LOG_ENTRY; j++){
		if( l->entry[j].latency > max ){
			max = l->entry[j].latency;
			i = j;
		}
	}

	my_buffer_offset += 
		sprintf(my_base + my_buffer_offset,
			"%6d %10s %8x %5d/%-15s %8x %5d/%s\n",
			(int)clock_to_usecs(l->entry[i].latency),
			(l->entry[i].cause == -99) ? "unknown" :
			(l->entry[i].cause < 0) ?
			irq_desc[~l->entry[i].cause].action ?
			irq_desc[~l->entry[i].cause].action->name 
			: "old irq" :
			(l->entry[i].cause > CAUSE_MAX) ? "Unknown" :
			cause_name[l->entry[i].cause],
			l->entry[i].mask,
			l->entry[i].start.FileLine,
			l->entry[i].start.FileName == (char *)0 ?
			"entry.S" : 
			l->entry[i].start.FileName,
			l->entry[i].start.Address,
			/*l->entry[i].start.ProcId,
			  l->entry[i].start.ProcName,*/
			l->entry[i].end.FileLine,
			l->entry[i].end.FileName == (char *)0 ?
			"entry.S" : 
			l->entry[i].end.FileName /*,
			l->entry[i].end.ProcId,
			l->entry[i].end.ProcName*/);

	/* Clearing these two effectivly clears the entry */

	l->entry[i].latency = 0;
	l->entry[i].end.Address = 0;
	*my_first_byte = page_buffer;
	return  my_buffer_offset;       
}

int __init latencytimes_init(void)
{
	readclock_init();
#ifdef CONFIG_PROC_FS
	create_proc_read_entry("latencytimes", 0, 0, latencytimes_read_proc, 0);
#endif

	reset_latencyData();
	return 0;
}

__initcall(latencytimes_init);
