/*
 * FILE NAME imaxoff.c
 *
 * BRIEF MODULE DESCRIPTION
 * This file implements the tracking of the places where interrupts
 * are turned off.
 *
 * Author: David Singleton 
 *	MontaVista Software, Inc.
 *      support@mvista.com
 *
 * Copyright 2001 MontaVista Software Inc.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
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
#include <asm/system.h>
#include <linux/time.h>
#include <linux/proc_fs.h>
#include <linux/init.h>
#include <linux/ilatency.h>
#include <asm/preem_latency.h>

unsigned ticks_per_usec;
static const char *cli_file;
static unsigned cli_line_no;
static unsigned cli_ticks;
static int latency_debug = 0;
static interrupt_latency_log_t latency = {
	0,
	"worst case interrupt hold off",
	0,
	0,
	0,

	1,
	0xffffffff,

	0,
	0,

	0,
	0,
	0,
	0,
	0,
	0,
	0
};

/*
 * on PPC and MIPs we branch-link here from ret_from_exception to
 * catch interrupts coming back on.  We also just log the fact that
 * interrupts are coming back on, we don't actually turn them on, thus
 * the third parameter, just_logging. 
 */

void
intr_ret_from_exception()
{
	intr_sti(__BASE_FILE__, __LINE__, 1);
}

/* 
 * If this is the outer most cli we log it.  If it's a nested cli, 
 * that is trying to turn interupts off and they are already off
 * we don't note the time or call to cli.
 * To keep our accounting straight, we set latency.sync to 1 in
 * this routine and to zero in intr_sti().  This helps us keep
 * track that we have noted the cli for this sti.  We want to
 * keep cli()/sti() pairs matched.  If you have any doubt, read the code
 * at the file and line numbers reported in /proc/imaxoff.  If they
 * are the right cli/sti pairs then there is yet another hole in
 * the instrumentation where the kernel is turning interrupts on
 * and off and we've missed it.
 */

void intr_cli(const char *cli_fname, unsigned cli_lineno)
{
	unsigned flag;

	__save_flags(flag); 
	__intr_cli();

	/* if we are not logging or we have an error, do nothing */
	if (latency.logging == OFF) {
		return;
	}
	
	/* 
 	 * If interrupts were already disabled then we don't
 	 * need to log it . . this is a nested interrupt. We
	 * want the outer pair of cli/sti.
	 */

	if (!INTERRUPTS_ENABLED(flag)) {
		latency.skip_cli++;
		return;
	}

	/* debug */
	if (latency.sync == 1) {
		latency.cli_error++;
	}
	latency.sync = 1;

	latency.iret_count = 0;

	/* Read the Time Stamp Counter */
	cli_file = cli_fname;
	cli_line_no = cli_lineno;
	readclock(cli_ticks);

	return;
}

/*
 * We want to make sure we have the sti that corresponds to the cli
 * caught above.  To do this we make sure the sync count is 1, as set
 * by intr_cli().  Originally iret_count was meant to catch returns
 * to userland.  If we'd caught an sti() in the kernel and somehow
 * we went back to userland before this sti() was called the cli/sti
 * pair is bogus.  I don't think iret_count ever worked.  I've kept it
 * for historical reasons and perhaps someday I can make it work.
 * The just_logging parameter is for those situations where we know
 * we are going to be turning interrupts back on in return from
 * exception. We grab the information for logging only but don't 
 * do the __sti we let the original code path do it.  An example is the
 * do_IRQ path for ARM.  We know the exception handler will turn it
 * back on, but it's such a nest of handlers that it's easier to just
 * log it in do_IRQ and leave the exception handlers alone.  We do
 * instrument the exception handlers for PPC and MIPs, they have simple
 * branch-link instructions so we can make the call just before
 * interrupts are turned back on.
 */

#define MAXINT	0x7fffffff

void intr_sti(const char *fname, unsigned lineno, int just_logging)
{
	unsigned long flag;
	unsigned long sti_ticks = 0;
	unsigned usecs;
	unsigned long old_lowest;
	int i, index;

	__save_flags(flag);

	/* 
	 * logging is turned off in the boot path until we have registered
	 * with /proc.
	 */

	if (latency.logging == OFF) {
		goto out;
	}

	/* 
	 * If interrupts are already on we don't have to log this call.
	 */

	if (INTERRUPTS_ENABLED(flag)) {
		latency.skip_sti++;
		goto out;
	}

	/*
	 * Sync gets set to 1 by intr_cli.  If it's not one then we don't
	 * have the right cli/sti pair.
	 */

	if (latency.sync != 1) {
		latency.sti_error++;
		goto out;
	}

	/*
	 * If iret_count is not zero we've gone to user land and this is
	 * not a valid cli/sti pair.  I pointed out that no one sets this,
	 * but the originator had no comment . . 
	 */

	if (latency.iret_count != 0) {
		latency.sti_break_error++;
		goto out;
	}

	readclock(sti_ticks);
	latency.sync = 0;

	/*
	 * reading the clock is platform dependent and clock_to_usecs
	 * is setup by each platform as well usually in asm/preem_latency.h.  
	 * get the time and log all the time for this cli/sti pair.
	 */

#ifdef CLOCK_COUNTS_DOWN
	usecs = clock_to_usecs(cli_ticks - sti_ticks);
#else
	usecs = clock_to_usecs(sti_ticks - cli_ticks);
#endif 
	inthoff_logentry(usecs);
	latency.total_ints++;

	/*
	 * find the lowest hold off time and substitute the lowest with 
	 * the new entry, iff the new entry is lower.
	 * We don't keep multiple entries for the same offending
	 * file and line pair.  Only one entry for printk, for example.
	 * But we want the highest hold off time shown for all
	 * occurances of printk . . . 
	 */

	old_lowest = MAXINT;
	for (index = i = 0; i < NUM_LOG_ENTRIES; i++) {
		if (old_lowest > latency.log[i].interrupts_off) {
			old_lowest = latency.log[i].interrupts_off;
			index = i;	
		}
		if ((lineno == latency.log[i].sti_line) &&
		    (cli_line_no == latency.log[i].cli_line) &&
		    (fname[0] == latency.log[i].sti_file[0]) &&      
		    (cli_file[0] == latency.log[i].cli_file[0])) {    

			if (usecs > latency.log[i].interrupts_off)
				latency.log[i].interrupts_off = usecs;
			latency.log[i].multiples++;
			goto out;
		}
	}
	if ((usecs < old_lowest) && (old_lowest != MAXINT)) {
		goto out;
	}

	latency.log[index].interrupts_off = usecs;
	latency.log[index].sti_file = fname;
	latency.log[index].sti_line = lineno;
	latency.log[index].sti_ticks = sti_ticks;
	latency.log[index].cli_file = cli_file;
	latency.log[index].cli_line = cli_line_no;
	latency.log[index].cli_ticks = cli_ticks;
	latency.log[index].multiples++;

	/*
	 * We have a couple of places where we instrument
	 * the code to inform us that interrupt are coming
	 * back on, without actually turning them one,
	 * like in arm/kernel/irq.c.  We just log it and
	 * let the original code turn the interrupts back on.
	 */
out:
	if (!just_logging)
		__intr_sti();
}

void intr_restore_flags(const char *fname, unsigned lineno, unsigned x)
{
	unsigned long flag;

	__save_flags(flag);

	if (latency.logging == OFF) {
		__intr_restore_flags(x);
		return;
	}

	if (!INTERRUPTS_ENABLED(flag) && INTERRUPTS_ENABLED(x)) {
		latency.restore_sti++;
		intr_sti(fname, lineno, 0);
	}

	if (INTERRUPTS_ENABLED(flag) && !INTERRUPTS_ENABLED(x)) {
		latency.restore_cli++;
		intr_cli(fname, lineno);
	}

	__intr_restore_flags(x);
}

static int g_read_completed = 0;

static int itimes_read_proc(
	char *page_buffer,
	char **my_first_byte,
	off_t virtual_start,
	int length,
	int *eof,
	void *data)
{
	int my_buffer_offset = 0;
	char * const my_base = page_buffer;
	int i;
	if (virtual_start == 0) {
		/* 
		 * Just been opened so display the header information 
		 * also stop logging  BUGBUG: stop logging while we are 
		 * reading, initialize the index numbers in the log array
		 */
		g_read_completed = 0;
		my_buffer_offset += sprintf(my_base + my_buffer_offset,
		   "Maximum Interrupt Hold Off Times and Callers\n");

	} else if (g_read_completed == (NUM_LOG_ENTRIES + latency_debug)) {
		 *eof = 1;
		 return 0;
	}

	if (latency_debug) {
		my_buffer_offset += sprintf(my_base + my_buffer_offset,
		     "iret_count %u sync %u skip_sti %u skip_cli %u\n"
		     "sti_error %u cli_error %u sti_break_error %u "
		     "restore_sti %u restore_cli %u\n",
		     latency.iret_count, latency.sync, latency.skip_sti,
		     latency.skip_cli, latency.sti_error, latency.cli_error,
		     latency.sti_break_error, latency.restore_sti,
		     latency.restore_cli);
		g_read_completed++;
	}
	for (i = 0; i < NUM_LOG_ENTRIES; i++) {
		my_buffer_offset += sprintf(my_base + my_buffer_offset,
		    "%d) %u us interrupts off\n\tcli call in file %s"
		    " (%d times)\n"
		    "\tcli call at line %5u\t\n\tsti call in file %s\n"
		    "\tsti call at line %5u\n"
		    "\tcli timer count %10u\n\tsti timer count %10u\n", 
		    i, latency.log[i].interrupts_off,
		    latency.log[i].cli_file, latency.log[i].multiples,
		    latency.log[i].cli_line, latency.log[i].sti_file, 
		    latency.log[i].sti_line, latency.log[i].cli_ticks, 
		    latency.log[i].sti_ticks);
		g_read_completed++;
	}

	*my_first_byte = page_buffer;
	return  my_buffer_offset;
}

int __init
holdoffs_init(void)
{

#ifdef CONFIG_SMP
	printk("Interrupt holdoff times are not supported on SMP systems.\n");
	latency.logging = OFF;
#else
#ifdef CONFIG_PROC_FS
	create_proc_read_entry("imaxoff", 0, 0, itimes_read_proc, 0);
#endif /* CONFIG_PROC_FS */
	readclock_init();
	if (ticks_per_usec) {
		printk("Interrupt holdoff maximums being captured.\n");
		latency.logging = ON;
	} else {
		printk("Interrupt Latency timer is NOT on! No data will be collected\n");
	}
#endif /* ! CONFIG_SMP */
	return 0;
}

__initcall(holdoffs_init);
