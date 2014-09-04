/*
 * include/linux/lock_break.h - lock breaking routines
 *
 * since in-kernel preemption can not occur while a lock is
 * held, we can drop and reacquire long-held locks when they are
 * in a natural quiescent state to further lower system latency.
 *
 * (C) 2001 Robert Love
 *
 */

#ifndef _LINUX_LOCK_BREAK_H
#define _LINUX_LOCK_BREAK_H

#include <linux/compiler.h>

/*
 * setting this to 1 will instruct debug_lock_break to
 * note when the expected lock count does not equal the
 * actual count. if the lock count is higher than expected,
 * we aren't dropping enough locks.  if it is 0, we are
 * wasting our time since the system is already preemptible.
 */
#ifndef DEBUG_LOCK_BREAK
#define DEBUG_LOCK_BREAK 0
#endif

#ifdef CONFIG_LOCK_BREAK

#define conditional_schedule_needed() (unlikely(current->need_resched))

#define conditional_schedule_needed_and_preemptable(n)	\
	(conditional_schedule_needed() && current->preempt_count == n)

/*
 * setting the task's state to TASK_RUNNING is nothing but paranoia,
 * in the case where a task is delinquent in properly putting itself
 * to sleep.  we should test without it.
 */
#define unconditional_schedule() do { \
	__set_current_state(TASK_RUNNING); \
	schedule(); \
} while(0)

#define conditional_schedule() do { \
	if (conditional_schedule_needed()) \
		unconditional_schedule(); \
} while(0)

#define break_spin_lock(n) do { \
	spin_unlock(n); \
	spin_lock(n); \
} while(0)

#define break_spin_lock_and_resched(n) do { \
	spin_unlock(n); \
	conditional_schedule(); \
	spin_lock(n); \
} while(0)

#if DEBUG_LOCK_BREAK
#define debug_lock_break(n) do { \
	if (current->preempt_count != n) \
		printk(KERN_ERR "lock_break: %s:%d: count was %d not %d\n", \
			__FILE__, __LINE__, current->preempt_count, n); \
} while(0)
#else
#define debug_lock_break(n)
#endif

#define DEFINE_LOCK_COUNT() int _lock_break_count = 0
#define TEST_LOCK_COUNT(n) (++_lock_break_count > (n))
#define RESET_LOCK_COUNT() _lock_break_count = 0

#else
#define unconditional_schedule()
#define conditional_schedule()
#define conditional_schedule_needed() 0
#define conditional_schedule_needed_and_preemptable(n) 0
#define break_spin_lock(n)
#define break_spin_lock_and_resched(n)
#define debug_lock_break(n)
#define DEFINE_LOCK_COUNT()
#define TEST_LOCK_COUNT(n) 0
#define RESET_LOCK_COUNT()
#endif

#endif /* _LINUX_LOCK_BREAK_H */
