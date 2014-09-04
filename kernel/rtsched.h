/*
 *  linux/kernel/rtsched.h
 *
 *  NOTE: This is a .h file that is mostly source, not the usual convention.
 *        It is coded this way to allow the depend rules to correctly set
 *        up the make file dependencies.  This is an alternate scheduler
 *        that replaces the core scheduler in sched.c.  It does not, however,
 *        replace most of the static support functions that call schedule.
 *        By making this an include file for sched.c, all of those functions
 *        are retained without the need for duplicate code and its attendant
 *        support issues.  At the same time, keeping it a seperate file allows
 *        diff and patch to work most cleanly and correctly.
 *
 *  Kernel scheduler and related syscalls
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *  Copyright (C) 2000, 2001 MontaVista Software Inc.
 *
 *  1998-12-28  Implemented better SMP scheduling by Ingo Molnar
 *  2000-03-15  Added the Real Time run queue support by George Anzinger
 *  2000-8-29   Added code to do lazy recalculation of counters 
 *              by George Anzinger
 */

/*
 * 'sched.c' is the main kernel file. It contains scheduling primitives
 * (sleep_on, wakeup, schedule etc) as well as a number of simple system
 * call functions (type getpid()), which just extract a field from
 * current-task
 */

#ifndef preempt_disable
#define preempt_disable()
#define preempt_enable()
#define preempt_is_disabled() 0
#define preempt_enable_no_resched()
#endif

/*
 * scheduler variables
 */
#define VERSION_DATE "<20011203.1609.50>"
/*
 * We align per-CPU scheduling data on cacheline boundaries,
 * to prevent cacheline ping-pong.
 */
static union {
	struct schedule_data {
		struct task_struct * curr;
		cycles_t last_schedule;
                struct list_head schedule_data_list;
                int cpu,effprio;
	} schedule_data;
	char __pad [SMP_CACHE_BYTES];
} aligned_data [NR_CPUS] __cacheline_aligned = { {{&init_task,0,{0,0},0,0}}};

#define cpu_curr(cpu) aligned_data[(cpu)].schedule_data.curr
static void newprio_ready_q(struct task_struct * tptr,int newprio);
#ifdef CONFIG_SMP
static void newprio_executing(struct task_struct *tptr,int newprio);
static struct list_head hed_cpu_prio __cacheline_aligned = 
                                                LIST_HEAD_INIT(hed_cpu_prio);
#endif
/*
 * task_on_rq tests for task actually in the ready queue.
 * task_on_runque tests for task either on ready queue or being executed
 * (by virtue of our seting a running tasks run_list.next to 1)
 */
#define task_on_rq(p) ((unsigned)p->run_list.next > 1)

static struct list_head rq[MAX_PRI+1]  ____cacheline_aligned;

static struct ready_queue {
        int recalc;            /* # of counter recalculations on SCHED_OTHER */
        int ticks;             /* # of ticks for all in SCHED_OTHER ready Q */
} runq ____cacheline_aligned;

/* set the bit map up with guard bits below.  This will result in
 * priority -1 if there are no tasks in the ready queue which will
 * happen as we are not putting the idle tasks in the ready queue.
 */
static struct {
        int guard;
        int rq_bit_ary[(MAX_PRI/32) +1];
}rq_bits = {-1,{0,0,0,0}};
#define rq_bit_map rq_bits.rq_bit_ary   
     
static int high_prio=0;

#define Rdy_Q_Hed(pri) &rq[pri]

#define PREEMPTION_THRESHOLD 1

#define NOT_RT 0   /* Use priority zero for non-RT processes */
#define last_schedule(cpu) aligned_data[(cpu)].schedule_data.last_schedule

struct kernel_stat kstat;

#ifdef CONFIG_SMP

/*
 * At the moment, we will ignor cpus_allowed, primarily because if it were
 * used, we would have a conflict in the runq.ticks count (i.e. since we
 * are not scheduleing some tasks, the count would not reflect what is
 * is really on the list).  Oh, and also, nowhere is there code in the
 * kernel to set cpus_allowed to anything but -1.  In the long run, we
 * would like to try seperate lists for each cpu, at which point 
 * cpus_allowed could be used to direct the task to the proper list.

 * Well, darn, now there is code that messes with cpus_allowed.  We will change
 * sometime soon....
 */

#define idle_task(cpu) (init_tasks[cpu_number_map(cpu)])
#define can_schedule(p,cpu) \
	((p)->cpus_runnable & (p)->cpus_allowed & (1 << cpu))

#else

#define idle_task(cpu) (&init_task)
#define can_schedule(p,cpu) (1)

#endif

void scheduling_functions_start_here(void) { }

/*
 * This is the function that decides how desirable a process is..
 * You can weigh different processes against each other depending
 * on what CPU they've run on lately etc to try to handle cache
 * and TLB miss penalties.
 *
 * Return values:
 *	 -1000: never select this
 *	     0: out of time, recalculate counters (but it might still be
 *		selected)
 *	   +ve: "goodness" value (the larger, the better)
 */

static inline int goodness(struct task_struct * p, int this_cpu, struct mm_struct *this_mm)
{
	int weight;

	/*
	 * goodness is NEVER called for Realtime processes!
	 * Realtime process, select the first one on the
	 * runqueue (taking priorities within processes
	 * into account).
	 
         */
	/*
	 * Give the process a first-approximation goodness value
	 * according to the number of clock-ticks it has left.
	 *
	 * Don't do any other calculations if the time slice is
	 * over or if this is an idle task.
	 */
	weight = p->counter;
	if (weight <= 0)
		goto out;
			
#ifdef CONFIG_SMP
	/* Give a largish advantage to the same processor...   */
	/* (this is equivalent to penalizing other processors) */
	if (p->processor == this_cpu)
		weight += PROC_CHANGE_PENALTY;
#endif

	/* .. and a slight advantage to the current MM */
	if (p->mm == this_mm || !p->mm)
		weight += 1;
	weight += 20 - p->nice;

out:
	return weight;
}

/*
 * the 'goodness value' of replacing a process on a given CPU.
 * positive value means 'replace', zero or negative means 'dont'.
 */
static inline int preemption_goodness(struct task_struct * prev, struct task_struct * p, int cpu)
{
	return goodness(p, cpu, prev->active_mm) - goodness(prev, cpu, prev->active_mm);
}

/*
 * This is ugly, but reschedule_idle() is very timing-critical.
 * We are called with the runqueue spinlock held and we must
 * not claim the tasklist_lock.
 */
static FASTCALL(void reschedule_idle(struct task_struct * p));

static void reschedule_idle(struct task_struct * p)
{
#ifdef CONFIG_SMP
	int this_cpu = smp_processor_id(), target_cpu;
	struct task_struct  *target_tsk;
        struct list_head *cptr;
        struct schedule_data *sch;
	int  best_cpu;

	/*
	 * shortcut if the woken up task's last CPU is
	 * idle now.
	 */
	best_cpu = p->processor;
	target_tsk = idle_task(best_cpu);
	if (cpu_curr(best_cpu) == target_tsk)
		goto preempt_now;
        /*
         * For real time, the choice is simple.  We just check
         * if the most available processor is working on a lower
         * priority task.  If so we bounce it, if not, there is
         * nothing more important than what we are doing.
         * Note that this will pick up any idle cpu(s) we may
         * have as they will have effprio of -1.
         */
        cptr = hed_cpu_prio.prev;
        sch = list_entry(cptr,
                         struct schedule_data,
                         schedule_data_list);
        target_tsk = sch->curr;
        if (p->effprio > sch->effprio){
                goto preempt_now;
        }
        /*
         * If all cpus are doing real time and we failed
         * above, then there is no help for this task.
         */
        if ( sch->effprio ) 
                goto out_no_target;               
	/*
         * Non-real time contender and one or more processors
         * doing non-real time things.

         * So we have a non-real time task contending among
         * other non-real time tasks on one or more processors
         * We know we have no idle cpus.
         */
	/*
	 * No CPU is idle, but maybe this process has enough priority
	 * to preempt it's preferred CPU.
	 */
	target_tsk = cpu_curr(best_cpu);
	if (target_tsk->effprio == 0 &&
            preemption_goodness(target_tsk, p, best_cpu) > 0)
		goto preempt_now;

        for (; cptr != &hed_cpu_prio;  cptr = cptr->prev ){
                sch =list_entry(cptr,
                                struct schedule_data,
                                schedule_data_list);
                if (sch->effprio != 0) 
                        break;
                if (sch->cpu != best_cpu){
                        target_tsk = sch->curr;
                        if ( preemption_goodness(target_tsk, p, sch->cpu) > 
                             PREEMPTION_THRESHOLD)
                                goto  preempt_now;
		}
               
	}
	
out_no_target:
	return;

preempt_now:
	target_cpu = target_tsk->processor;
	target_tsk->need_resched = 1;
	/*
	 * the APIC stuff can go outside of the lock because
	 * it uses no task information, only CPU#.
	 */
	if ((target_cpu != this_cpu)
            && (target_tsk != idle_task(target_cpu)))
		smp_send_reschedule(target_cpu);
	return;
#else /* UP */
	struct task_struct *tsk;

	tsk = cpu_curr(0);
	if ((high_prio > tsk->effprio) ||
            (!tsk->effprio && preemption_goodness(tsk, p, 0) > 
             PREEMPTION_THRESHOLD)){
		tsk->need_resched = 1;
	}
#endif
}

/*
 * This routine maintains the list of smp processors.  This is 
 * a by directional list maintained in priority order.  The above
 * code used this list to find a processor to use for a new task.
 * The search will be backward thru the list as we want to take 
 * the lowest prioity cpu first.  We put equal prioities such that
 * the new one will be ahead of the old, so the new should stay
 * around a bit longer. 
 */

#ifdef CONFIG_SMP
static inline void re_queue_cpu(struct task_struct *next,
                                struct schedule_data *sch)
{
        struct list_head *cpuptr;
        list_del(&sch->schedule_data_list);
        sch->effprio = next->effprio;
        cpuptr = hed_cpu_prio.next;
        while (cpuptr != &hed_cpu_prio &&
               sch->effprio < list_entry(cpuptr,
                                         struct schedule_data,
                                         schedule_data_list)->effprio
                ) 
                cpuptr = cpuptr->next;
        list_add_tail(&sch->schedule_data_list,cpuptr);
        next->newprio = &newprio_executing;
}
#else
#define re_queue_cpu(a,b)
#endif
/*
 * Careful!
 *
 * This has to add the process to the _beginning_ of the
 * run-queue, not the end. See the comment about "This is
 * subtle" in the scheduler proper..
 * 
 * For real time tasks we do this a bit differently.  We 
 * keep a priority list of ready tasks.  We remove tasks 
 * from this list when they are running so a running real
 * time task will not be in either the ready list or the run
 * queue.  Also, in the name of speed and real time, only
 * priority is important so we spend a few bytes on the queue.
 * We have a doubly linked list for each priority.  This makes
 * Insert and removal very fast.  We also keep a bit map of
 * the priority queues where a bit says if the queue is empty
 * or not.  We also keep loose track of the highest priority
 * queue that is currently occupied.  This high_prio mark 
 * is updated when a higher priority task enters the ready
 * queue and only goes down when we look for a task in the
 * ready queue at high_prio and find none.  Then, and only
 * then, we examine the bit map to find the true high_prio.
 */

#define BF 31  /* bit flip constant */
#define   set_rq_bit(bit)    set_bit(BF-((bit)&0x1f),&rq_bit_map[(bit) >> 5])
#define clear_rq_bit(bit)  clear_bit(BF-((bit)&0x1f),&rq_bit_map[(bit) >> 5])

static inline void _del_from_runqueue(struct task_struct * p)
{
	nr_running--;
        list_del( &p->run_list );
        if (list_empty(Rdy_Q_Hed(p->effprio))){
                clear_rq_bit(p->effprio);
        }
	/*                  p->run_list.next = NULL; !=0 prevents requeue */
	p->run_list.next = NULL;
        p->newprio = NULL;
        if( !p->effprio) runq.ticks -= p->counter;
}
/* Exported for main.c, also used in init code here */
void __del_from_runqueue(struct task_struct * p)
{
        _del_from_runqueue(p);
}
static inline struct task_struct * get_next_task(struct task_struct * prev,
                                                 int this_cpu)
{
        struct list_head *next, *rqptr;
        struct task_struct *it=0;
        int *i,c,oldcounter;

 repeat_schedule:
        rqptr = Rdy_Q_Hed(high_prio);
        next = rqptr->next;
        if (unlikely( next == rqptr)){ 
                for (i=&rq_bit_map[MAX_PRI/32],high_prio=BF+((MAX_PRI/32)*32);
                     (*i == 0);high_prio -=32,i--);
                high_prio -= ffz(~*i);
                if (unlikely(high_prio < 0)){
                        /*
                         * No tasks to run, return this cpu's idle task 
                         * It is not in the ready queue, so no need to remove it.
                         * But first make sure its priority keeps it out of 
                         * the way.
                         */
                        high_prio = 0;
                        it = idle_task(this_cpu);
                        it->effprio = -1;
                        return it;
                }
                goto repeat_schedule;
        }
        /*
         * If there is only one task on the list, it is a no brainer.
         * But really, this also prevents us from looping on recalulation
         * if the one and only task is trying to yield.  These sort of 
         * loops are NOT_FUN.  Note: we use likely() to tilt toward 
         * real-time tasks, even thou they are, usually unlikely.  We
         * are, after all, a real time scheduler.
         */
        if ( likely(high_prio || next->next == rqptr)){ 
                it = list_entry(next, struct task_struct, run_list);
 back_from_figure_non_rt_next:
                _del_from_runqueue(it);
                return it;               
        }
        /*
         * Here we set up a SCHED_OTHER yield.  Note that for other policies
         * yield is handled else where.  This means we can use == and = 
         * instead of & and &= to test and clear the flag.  If the prev 
         * task has all the runq.ticks, then we just do the recaculation
         * version and let the winner take all (yield fails).  Otherwise
         * we fource the counter to zero for the loop and put it back
         * after we found some other task.  We must remember to update
         * runq.ticks during all this.  Also, we don't give it all back
         * if the yielder has more than the next guy.
         */
        oldcounter = 0;
        if ( unlikely(prev->policy == (SCHED_YIELD | SCHED_OTHER)) ){
                if ( unlikely(prev->counter == runq.ticks)) {
                        prev->policy = SCHED_OTHER;
                        runq.ticks = 0;
                }else{
                        oldcounter = prev->counter;
                        prev->counter = 0;
                }
        }
        c = -1000;
        if (likely(runq.ticks > 0)) {
                do {
                        int weight;
                        struct task_struct *p = 
                                list_entry(next, struct task_struct, run_list);
                        /* if (can_schedule(p, this_cpu))*/ {
                                weight = goodness(p, this_cpu, prev->active_mm);
                                if (weight > c)
                                        c = weight, it = p;
                        }
                        next = next->next;
                } while (next != rqptr);
                /*
                 * if we get out of sync with the runq.ticks counter 
                 * force it to 0 and catch it next time around. Note we
                 * catch a negative counter on entry.
                 */
                if ( unlikely(c <= 0 )){ 
                        runq.ticks = 0;
                }
        }else{
#ifdef CONFIG_SMP
                /*
                 * Here we update the tasks that are current on other 
                 * processors
                 */
                struct list_head *wkptr,
                        *cptr=&aligned_data[(this_cpu)].
                        schedule_data.
                        schedule_data_list;

                runq.ticks = 0;
                list_for_each ( wkptr, &hed_cpu_prio) {
                        struct task_struct *p;
                        if (cptr == wkptr ) continue;
                        p = list_entry(wkptr,
                                       struct schedule_data, 
                                       schedule_data_list)->curr;
                        if ( p->effprio == 0){
                                p->counter = (p->counter >> 1) + 
                                        NICE_TO_TICKS(p->nice);
                                p->counter_recalc++;
                        }
                }
#else
                runq.ticks = 0;
#endif
                runq.recalc++;
                do {
                        int weight;
                        struct task_struct *p = 
                                list_entry(next, struct task_struct, run_list);
                        runq.ticks += 
                                p->counter = NICE_TO_TICKS(p->nice);
                        p->counter_recalc++;
                        /* if (can_schedule(p, this_cpu)) */
                        {
                                weight = goodness(p, this_cpu, prev->active_mm);
                                if (weight > c)
                                        c = weight, it = p;
                        }
                        next = next->next;
                } while (next != rqptr);
        }
        /* Undo the stuff we did for SCHED_YIELD.  We know we did something
         * if oldcounter != 0.
         */
        if (unlikely(oldcounter)){
                
                prev->counter = (it->counter < oldcounter) ? 
                        it->counter : 
                        oldcounter;
                runq.ticks += prev->counter-oldcounter;
                prev->policy &= ~SCHED_YIELD;
        }
        goto back_from_figure_non_rt_next;

}
/* Add to the head of the run queue */
static inline void add_to_runqueue(struct task_struct * p,int cpu)
{
	struct list_head *next;
        int prio;
        /* idle tasks, don't get put in the list */
        if (unlikely(p == idle_task(cpu))) return;  
        prio = p->effprio;
        next = Rdy_Q_Hed(prio);
        if (list_empty(next)) { /* an empty queue */
                set_rq_bit(prio);
                if (high_prio < prio) {
                        high_prio = prio;
                }
        }
        list_add(&p->run_list,next);
        p->newprio = newprio_ready_q;
        if ( likely(!p->effprio )) {
                int diff,c;
		if ((diff = runq.recalc - p->counter_recalc) != 0) {
 			p->counter_recalc = runq.recalc;
			c = NICE_TO_TICKS(p->nice) << 1;
			p->counter = diff > 8 ? c - 1 :  /* max priority */
				                c + ((p->counter - c) >> diff);
		}
                runq.ticks += p->counter;
        }
	nr_running++;
}

/*
 * This function is only called from schedule() so it need not worry
 * about updating the counter as it should never be out of date.
 * If you change this, remember to do the update.
 */
static inline void add_last_runqueue(struct task_struct * p)
{
	struct list_head *next = Rdy_Q_Hed(p->effprio);

        if (list_empty(next)) {         /* empty list, set the bit */
                set_rq_bit(p->effprio);
                if (p->effprio > high_prio){
                        high_prio = p->effprio;
                }
        }
        list_add_tail(&p->run_list,next);
        p->newprio = newprio_ready_q;
        if ( !p->effprio ) runq.ticks += p->counter;
        nr_running++;
}


static inline void move_first_runqueue(struct task_struct * p)
{
	list_del(&p->run_list);
	list_add_tail(&p->run_list, Rdy_Q_Hed(p->effprio));
}
/*
 * When we have a task in some queue by priority, we need
 * to provide a way to change that priority.  Depending on the
 * queue we must do different things.  We handle this by putting
 * a function address in the task_struct (newprio()).
 *
 * First a front end routine to take care of the case were the task
 * is not in any priority queues.  We take the runqueue_lock
 * here, so the caller must not.  Since we may be called
 * recursively, protect against a dead lock.
 */
static struct task_struct *newprio_inuse;
static int newprio_inuse_count;

void set_newprio(struct task_struct * tptr, int newprio)
{
	if ( newprio_inuse != current){
                spin_lock_irq(&runqueue_lock);
                newprio_inuse = current;
        }
        newprio_inuse_count++;
        if (! tptr->newprio ) {
                tptr->effprio = newprio;
        }else if ( tptr->effprio != newprio) {
                tptr->newprio(tptr,newprio);
        }
        if ( ! --newprio_inuse_count ){
                spin_unlock_irq(&runqueue_lock);
                newprio_inuse = 0;
        }
}


/*
 * Here are the routines we use for the ready queue and an executing
 * process.  Note that the executing process may fall out of favor
 * as a result of the change.  We do the right thing. Note that newprio
 * is not cleared so we test here to see if the task is still running.
 */

static void newprio_ready_q(struct task_struct * tptr,int newprio)
{
        _del_from_runqueue(tptr);
        tptr->effprio = newprio;
	add_to_runqueue(tptr,0);
	reschedule_idle(tptr);
}
#ifdef CONFIG_SMP
static void newprio_executing(struct task_struct *tptr,int newprio)
{
        int cpu;
        struct schedule_data *sched_data;
        if(!newprio || newprio < tptr->effprio){
		tptr->need_resched = 1;
        }
        cpu = tptr->processor;
	sched_data = & aligned_data[cpu].schedule_data;
        tptr->effprio = newprio;
        if( sched_data->curr != tptr) return; /* if not expected, out of here */
        re_queue_cpu(tptr,sched_data);
	if ((cpu != smp_processor_id()) && tptr->need_resched)
		smp_send_reschedule(cpu);
}
#endif



/*
 * Wake up a process. Put it on the ready-queue if it's not
 * already there.  The "current" process is not on the 
 * ready-queue (it makes it much easier to figure out if we
 * need to preempt, esp. the real time case).  It is possible
 * to wake the current process.  This happens when it is waken
 * before schedule has had a chance to put it properly to
 * sleep.  If schedule did not turn on ints in the middle of
 * things this would all be ok, however, it does so we have the
 * possibility of being in that window.  
 * The "current" process is never on the
 * run-queue (except when the actual re-schedule is in
 * progress), and as such you're allowed to do the simpler
 * "current->state = TASK_RUNNING" to mark yourself runnable
 * without the overhead of this.
 */
static inline int try_to_wake_up(struct task_struct * p, int synchronous)
{
	unsigned long flags;
	int success = 0;

	TRACE_PROCESS(TRACE_EV_PROCESS_WAKEUP, p->pid, p->state);

	/*
	 * We want the common case fall through straight, thus the goto.
	 */
	spin_lock_irqsave(&runqueue_lock, flags);
	p->state = TASK_RUNNING;
	if ( task_on_runqueue(p) )
		goto out;
	add_to_runqueue(p,0);
	if (!synchronous /*|| !(p->cpus_allowed & (1 << smp_processor_id())*/)
		reschedule_idle(p);
	success = 1;
out:
	spin_unlock_irqrestore(&runqueue_lock, flags);
	return success;
}

inline int wake_up_process(struct task_struct * p)
{
	return try_to_wake_up(p, 0);
}
/*
 * schedule_tail() is getting called from the fork return path. This
 * cleans up all remaining scheduler things, without impacting the
 * common case.
 */
static inline void __schedule_tail(struct task_struct *prev)
{
#ifdef CONFIG_SMP

	/*
	 * fast path falls through. We have to clear cpus_runnable before
	 * checking prev->state to avoid a wakeup race. Protect against
	 * the task exiting early.
	 */
	task_lock(prev);
	task_release_cpu(prev);
	mb();
        if (task_on_rq(prev))
		goto needs_resched;

out_unlock:
	task_unlock(prev);	/* Synchronise here with release_task() if prev is TASK_ZOMBIE */
	return;

	/*
	 * Slow path - we 'push' the previous process and
	 * reschedule_idle() will attempt to find a new
	 * processor for it. (but it might preempt the
	 * current process as well.) We must take the runqueue
	 * lock and re-check prev->state to be correct. It might
	 * still happen that this process has a preemption
	 * 'in progress' already - but this is not a problem and
	 * might happen in other circumstances as well.
	 */
needs_resched:
	{
		unsigned long flags;

		/*
		 * Avoid taking the runqueue lock in cases where
		 * no preemption-check is necessery:
                  * Note: Idle task is NEVER on the ready queue so
                 *       no need to check if prev was idle.
		 */

		spin_lock_irqsave(&runqueue_lock, flags);
		if (task_on_rq(prev) /* && !task_has_cpu(prev)*/ )
			reschedule_idle(prev);
		spin_unlock_irqrestore(&runqueue_lock, flags);
		goto out_unlock;
	}
#define smp_label_a _smp_label_a:
#define smp_label_b _smp_label_b:
#else
	prev->policy &= ~SCHED_YIELD;
#define smp_label_a
#define smp_label_b 
#endif /* CONFIG_SMP */
}

asmlinkage void schedule_tail(struct task_struct *prev)
{
	__schedule_tail(prev);
	preempt_enable();
}

/*
 *  'schedule()' is the scheduler function. It's a very simple and nice
 * scheduler: it's not perfect, but certainly works for most things.
 *
 * The goto is "interesting".
 *
 *   NOTE!!  Task 0 is the 'idle' task, which gets called when no other
 * tasks can run. It can not be killed, and it cannot sleep. The 'state'
 * information in task[0] is never used.
 */
asmlinkage void schedule(void)
{
	struct schedule_data * sched_data;
	struct task_struct *prev, *next;
	int this_cpu;

	spin_lock_prefetch(&runqueue_lock);
 try_try_again:

	preempt_disable(); 

	if (unlikely(!current->active_mm)) BUG();
	prev = current;
	this_cpu = prev->processor;

	if (unlikely(in_interrupt())) {
		printk("Scheduling in interrupt\n");
		BUG();
	}

	release_kernel_lock(prev, this_cpu);

	/*
	 * 'sched_data' is protected by the fact that we can run
	 * only one process per CPU.
	 */
	sched_data = & aligned_data[this_cpu].schedule_data;

	spin_lock_irq(&runqueue_lock);

#ifdef CONFIG_PREEMPT
        /*
         * Note that this is an '&' NOT an '&&'...
         */
        if (preempt_is_disabled() & PREEMPT_ACTIVE) goto sw_TASK_RUNNING;
#endif	
	if (prev->state == TASK_INTERRUPTIBLE) {
		//case TASK_INTERRUPTIBLE:
                if (likely( ! signal_pending(prev))) {
                        goto sw_default;
                }
                prev->state = TASK_RUNNING;
        }

	if (prev->state != TASK_RUNNING) {
                goto sw_default;
        }
        //case TASK_RUNNING:
#ifdef CONFIG_PREEMPT
 sw_TASK_RUNNING:
#endif
        /*
         * move an exhausted RR process to be last.. 
         * Do the same for Yields
         */
        if (!prev->counter && (prev->policy & SCHED_RR))
                goto move_rr_last;
        if (prev->policy & SCHED_YIELD)
                goto move_yield_last;
        /*
         * There is a case where current is already
         * in the ready que.  That is where it was
         * on the way out, but the wait already
         * expired, so wake_up_process has already
         * done it.  In this case, we don't!!
         */
        if (!task_on_rq(prev))
                add_to_runqueue(prev,this_cpu);
        goto move_rr_back;
        //default:
 sw_default:
        prev->sleep_time = jiffies;
        prev->run_list.next = 0;
	
 move_rr_back:
	prev->need_resched = 0;
 smp_label_a
        next = get_next_task(prev, this_cpu);
 smp_label_b
        next->run_list.next = (struct list_head *)1;
        sched_data->curr = next;
        re_queue_cpu(next,sched_data);
	spin_unlock_irq(&runqueue_lock);

	if (unlikely(prev == next)) {
		goto same_process;
	}

#ifdef CONFIG_SMP
 	/*
 	 * maintain the per-process 'last schedule' value.
 	 * (this has to be recalculated even if we reschedule to
 	 * the same process) Currently this is only used on SMP,
	 * and it's approximate, so we do not have to maintain
	 * it while holding the runqueue spinlock.
 	 */
 	sched_data->last_schedule = get_cycles();

	/*
	 * We drop the scheduler lock early (it's a global spinlock),
	 * thus we have to lock the previous process from getting
	 * rescheduled during switch_to() (since we are still on his stack).
         *
         * Here is how we do it.  The cpus_runnable flag will be held until
         * the task is truly available.  On the other hand, this task
         * is put in the ready queue during the above runqueue_lock so
         * it may be picked up by another cpu.  Suppose that cpu is this
         * one.  Now the prior cpu left the task in the ready queue and
         * we have just pluck it from there.  No conflict so far, but if
         * cpus_runnable is not clear, the other cpu is still in the switch code.
         * There are no locks there SAVE THIS ONE!!!  Oh woe is me!  
         * At the same time, under these conditions, i.e. a task is
         * coming out of the ready queue before we actually switch, it
         * would be good to not switch cpus.  So lets define a "wanted"
         * bit in the cpus_runnable member.  Oops, it is now a cpu bit mask
         * so, since only a few folks look at it, we will fudge it a bit.  
         * Choose an addition that is more than on bit away from a single bit
         * 

         * We will spin here waiting for cpus_runnable to go to zero.  Until
         * this happens, we must not change the processor value as
         * interrupt code depends on this being right for "current".
	 */
#define WANTED 10
#define TAKEN  20
        {
                unsigned long cur_cpus_runnable = next->cpus_runnable;

                atomic_add(WANTED,(atomic_t *)&next->cpus_runnable);
                /*
                 * It is either "WANTED+cur_cpus_runnable" which means we 
                 * need to wait or is:
                 * A. The old cpu_id + WANTED or
                 * B. WANTED - 1 which means it cleared (or was clear).
                 * C. TAKEN + cur_cpus_runnable
                 */
                while ((cur_cpus_runnable != ~0UL) && 
                       (volatile int)next->cpus_runnable == 
                       WANTED + cur_cpus_runnable) {
                        unsigned long my_cpu = 1 << this_cpu;

                        barrier();
                        /*
                         * OK, so while we wait, lets look in on prev and see
                         * if he is wanted.
                         */
                        if ( (volatile int)prev->cpus_runnable !=  my_cpu) {
                                /*
                                 * Another cpu wants the task we have yet to 
                                 * switch away from.  Lets steal it back.
                                 * Once WANTED is set on prev, we can clear it 
                                 * either here or in schedule_tail.  The other
                                 * cpu can clear it by coming here where it will
                                 * be known by him as next...
                        
                                 * Here, we set it to (TAKEN+my_cpu), in 
                                 * schedule_tail it is set to my_cpu
                                 */
                                spin_lock_irq(&runqueue_lock);
                                if ( (volatile int)prev->cpus_runnable != my_cpu) {
                                        spin_unlock_irq(&runqueue_lock);
                                        continue;
                                }
                                /*
                                 * Three possibilities on the state of next:
                                 * 0.) cpus_runnable has gone to ~0UL.  Means the
                                 *     prior cpu has finished and is not
                                 *     interested.  So put back in ready queue.
                                 * 5.) Other cpu noticed our interest and stoled
                                 *     it back (cpus_runnable will be 
                                 *     TAKEN + his flag).  Do nothing.
                                 * 3.) No change, put back in the ready queue
                                 * Note, case 3 presents a bit of a race on our
                                 * clearing the WANTED bit.  So, we subtract and
                                 * if the result is negative, set it to zero.
                                 */
                                if ( (volatile int)next->cpus_runnable != 
                                     cur_cpus_runnable + TAKEN) {
                                        atomic_add(-WANTED,
                                                   (atomic_t *)&next->cpus_runnable);
                                        if ((volatile int)next->cpus_runnable < 0) {
                                                next->cpus_runnable = ~0UL;
                                        }
                                        add_to_runqueue(next,this_cpu);
                                }
                                /*
                                 * So much for "next".  Now lets take prev.
                                 * Setting cpus_runnable to TAKEN+old will pop the
                                 * waiter out of the wait loop.
                                 * We then wait for him to clear TAKEN to 
                                 * complete the handshake.  We hand shake here
                                 * to keep the other cpu from seeing some later
                                 * state that may be wrong.
                                 */
                                prev->cpus_runnable = TAKEN + my_cpu; 
                                next = prev;
                                spin_unlock_irq(&runqueue_lock);
                                while ((volatile int)prev->cpus_runnable == 
                                       TAKEN + my_cpu) {
                                        barrier();
                                }
                                spin_lock_irq(&runqueue_lock);
                                goto _smp_label_b;
                        }
                }              
                /*
                 * if we poped out of the while because cpus_runnable has TAKEN
                 * set it means the prior owner stoled back the task.  Time to 
                 * rescan the ready queue (after clearing the TAKEN bit to
                 * complete the handshake).  The other possibilities are:
                 * cpus_runnable = WANTED -1 ( was clear when we started)
                 * cpus_runnable = -1      (was his, but the other cpu finished, 
                 *                          seting -1)
                 */
                if ((volatile int)next->cpus_runnable == 
                    TAKEN + cur_cpus_runnable){
                        atomic_add(-TAKEN,(atomic_t *)&next->cpus_runnable);
                        spin_lock_irq(&runqueue_lock);
                        goto _smp_label_a;
                }
        }
        /*
         * Gosh wasn't that fun!
         */
	task_set_cpu(next,this_cpu);
#endif /* CONFIG_SMP */

        /* 
         * An interesting problem here. Since we turned on interrupts,
         * we could now have a need schedule flag set in prev.  Actually
         * this can only happen on interrupt and then only be meaningful
         * if it is done by a wakeup() call to reschedule_idle().  This
         * is covered as that code will set the need_resched flag in the
         * task found by cpu_curr() which comes from the cpu structs
         * which we have already updated.

         * The remaining problems come from left over timeouts against
         * prev, but he was the target and he is gone now... unless
         * we did not really switch.  So in the switch path we will 
         * clear the need_resched flag, not in the no switch path.
         */

	kstat.context_swtch++;
	/*
	 * there are 3 processes which are affected by a context switch:
	 *
	 * prev == .... ==> (last => next)
	 *
	 * It's the 'much more previous' 'prev' that is on next's stack,
	 * but prev is set to (the just run) 'last' process by switch_to().
	 * This might sound slightly confusing but makes tons of sense.
	 */
	prepare_to_switch();
	{
		struct mm_struct *mm = next->mm;
		struct mm_struct *oldmm = prev->active_mm;
		if (!mm) {
			if (next->active_mm) BUG();
			next->active_mm = oldmm;
			atomic_inc(&oldmm->mm_count);
			enter_lazy_tlb(oldmm, next, this_cpu);
		} else {
			if (next->active_mm != mm) BUG();
			switch_mm(oldmm, mm, next, this_cpu);
		}

		if (!prev->mm) {
			prev->active_mm = NULL;
			mmdrop(oldmm);
		}
	}

	TRACE_SCHEDCHANGE(prev->pid, next->pid, prev->state);

	/*
	 * This just switches the register state and the
	 * stack.
	 */
	switch_to(prev, next, prev);
	__schedule_tail(prev);
	prev->need_resched = 0;

same_process:
        reacquire_kernel_lock(current);
	preempt_enable_no_resched(); 
        if ( ! current->need_resched)
                return;

        /* The task managed to get its need_resched flag set already!
         */
        goto try_try_again;


 move_rr_last:
        prev->counter = NICE_TO_TICKS(prev->nice);

 move_yield_last:
        if (prev->effprio)    /* non-real time tasks get cleared later */
                prev->policy &= ~SCHED_YIELD;
        add_last_runqueue(prev);
	goto move_rr_back;

}
static inline struct task_struct *find_process_by_pid(pid_t pid);

static int setscheduler(pid_t pid, int policy, 
			struct sched_param *param)
{
	struct sched_param lp;
	struct task_struct *p;
	int retval;

	retval = -EINVAL;
	if (!param || pid < 0)
		goto out_nounlock;

	retval = -EFAULT;
	if (copy_from_user(&lp, param, sizeof(struct sched_param)))
		goto out_nounlock;

	/*
	 * We play safe to avoid deadlocks.
	 */
	read_lock_irq(&tasklist_lock);
	spin_lock(&runqueue_lock);

	p = find_process_by_pid(pid);

	retval = -ESRCH;
	if (!p)
		goto out_unlock;
			
	if (policy < 0)
		policy = p->policy;
	else {
		retval = -EINVAL;
		if (policy != SCHED_FIFO && policy != SCHED_RR &&
				policy != SCHED_OTHER)
			goto out_unlock;
	}
	
	/*
	 * Valid priorities for SCHED_FIFO and SCHED_RR are 1..MAX_PRI, valid
	 * priority for SCHED_OTHER is 0.
	 */
	retval = -EINVAL;
	if (lp.sched_priority < 0 || lp.sched_priority > MAX_PRI)
		goto out_unlock;
	if ((policy == SCHED_OTHER) != (lp.sched_priority == 0))
		goto out_unlock;

	retval = -EPERM;
	if ((policy == SCHED_FIFO || policy == SCHED_RR) && 
	    !capable(CAP_SYS_NICE))
		goto out_unlock;
	if ((current->euid != p->euid) && (current->euid != p->uid) &&
	    !capable(CAP_SYS_NICE))
		goto out_unlock;

	retval = 0;
	p->policy = policy;
        if ( policy == SCHED_FIFO) {
                p->counter = -100;        /* we don't count down neg couters */
        }else{
                p->counter = NICE_TO_TICKS(p->nice);
        }

        p->rt_priority = lp.sched_priority;

	spin_unlock_irq(&runqueue_lock);
        set_newprio(p,lp.sched_priority);
        goto out_readunlock;
                        
out_unlock:
	spin_unlock_irq(&runqueue_lock);
 out_readunlock:
	read_unlock(&tasklist_lock);

out_nounlock:
	return retval;
}
asmlinkage long sys_sched_yield(void)
{
	/*
	 * Trick. sched_yield() first checks to see if it will be REALLY
         * lonly in the ready queue. It just returns if it is the only
         * game in town. The multilple ready queues really help here.
         * (This test does not have
	 * to be atomic.) In threaded applications this optimization
	 * gets triggered quite often.
	 */
        if ( ! list_empty(Rdy_Q_Hed(current->effprio))){
                /* 
                 * I think this is safe as only the current task can 
                 * here and only the current task will be clearing this bit
                 */
                current->policy |= SCHED_YIELD;
                schedule();
        }
	return 0;
}
/* Seems to be the first place we hear about a given cpu as it comes up.
 * A new (including the first) cpu is reporting for duty.  Since he is
 * already running we must patch him into the processor queue.  
 * We get here the first time the processor enters the idle code and also
 * one more time for the boot cpu so... be careful to not redo what is 
 * already done.  Also note that the fork that created the task put it
 * in the ready queue, so we need to take it out, except the initial cpus
 * task was not created by a fork.  No matter, the removal code works even 
 * then.
 * We give the idle task prioity -1 to keep it out of the way of tasks 
 * that have real work to do.
 */
extern unsigned long wait_init_idle;

void __init init_idle(void)
{
	struct schedule_data * sched_data;
        int cpu=smp_processor_id();
	sched_data = &aligned_data[cpu].schedule_data;

        if (task_on_rq(current)) {
                del_from_runqueue(current);
        }
	sched_data->curr = current;
	sched_data->last_schedule = get_cycles();
        current->effprio = current->rt_priority = 0;
        sched_data->effprio = -1;        /* idle flag */
        sched_data->cpu = cpu;
	clear_bit(current->processor, &wait_init_idle);
#ifdef CONFIG_SMP
        if ( ! sched_data->schedule_data_list.next ) {
                list_add_tail(&sched_data->schedule_data_list,&hed_cpu_prio);
        }
#endif
#ifdef CONFIG_PREEMPT
	if (current->processor)
		current->preempt_count = 0;
#endif
}

extern void init_timervecs (void);

void __init sched_init(void)
{
	/*
	 * We have to do a little magic to get the first
	 * process right in SMP mode.
	 */
	int cpu = smp_processor_id();
	int nr;
        int i;

	init_task.processor = cpu;
        /* Init the ready queue */
        for (i=0;i<=MAX_PRI ;i++){
                INIT_LIST_HEAD(Rdy_Q_Hed(i));
        }


	for(nr = 0; nr < PIDHASH_SZ; nr++)
		pidhash[nr] = NULL;
        printk("rtsched version " VERSION_DATE "\n");

	init_timervecs();

	init_bh(TIMER_BH, timer_bh);
	init_bh(TQUEUE_BH, tqueue_bh);
	init_bh(IMMEDIATE_BH, immediate_bh);

	/*
	 * The boot idle thread does lazy MMU switching as well:
	 */
	atomic_inc(&init_mm.mm_count);
	enter_lazy_tlb(&init_mm, current, cpu);
}
