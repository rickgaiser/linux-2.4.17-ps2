/*
 * stacktrace.c - mips stack backtrace support
 *
 *  Copyright (C) 2000, 2001  Sony Computer Entertainment Inc.
 *  Copyright 2002  Sony Corportaion.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  version 2 of the  License.
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

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/vmalloc.h>
#include <asm/inst.h>


#define REG_K0	26
#define REG_K1	27
#define REG_RA	31
#define REG_FP	30
#define REG_SP	29
#define REG_GP	28

#ifndef get_gpreg
#define get_gpreg(regs, index) ((regs)->regs[(index)])
#endif

#define get_stack_lower(taskp) ((taskp) + sizeof (struct task_struct))
#define get_stack_upper(taskp) ((taskp) + KERNEL_STACK_SIZE -32 -1)

#define DECL_SYMBOL_STRING_SPACE(name)	char name[1];
#define INIT_SYMBOL_STRING_SPACE(name)	(name)[0]='\0';
#define sprint_symbol(str, sz, addr)	do { ; } while(0)

#define INST_SIZE	(sizeof(union mips_instruction))

#ifdef DEBUG
static
void dump_stack (unsigned long sp)
{
    int i;
    for(i = -32; i < 32; i += 4 )
	printk("%s %8.8lx %8.8lx\n", i==0?">":" ",
		(sp + i), *(__u32 *)(sp + i));
}
#endif


/* Return ture, if given P is valid */
static
inline int is_aligned (unsigned long p)
{
    return ((p&0x3)==0);
}

/* Return ture, if given P is valid */
static
inline int is_kern_area (unsigned long p)
{
    const unsigned long kernel_start = KSEG0;
    const unsigned long kernel_end = KSEG1-0x4;
    const unsigned long module_start = VMALLOC_START;
    const unsigned long module_end = VMALLOC_END;

    return (is_aligned(p)  
    		&& ((p >= kernel_start && p < kernel_end)
    		    || (p >= module_start && p < module_end)));
}


/* Get value of *ADDR into *VALP and return ZERO.  Return -EFAULT, 
   if accessing *ADDR failed.*/
static
inline int get_kern_word (__u32 *addr, __u32 *valp)
{
    __u32 val;
    int error = -EFAULT;

    asm(
      "1:\n\t"
	"lw	%0, (%2);\n\t"
	"move	%1, $0;\n\t"
	".section	__ex_table,\"a\";\n\t"
	".word	1b, %3;\n\t"
	".previous;\n\t"
	: "=r"(val), "=r"(error) 
	: "r"(addr), "i"(&&out));
    *valp = val;
out:
    return error;
}

/*
 * Assume that caller have following codes...
 * 
 * 	jal	xxx 
 * or
 * 	lui	Rx, %hi(xxx)
 * 	lw	Ry, %lo(xxx)(Rx)
 *	nop
 * 	jalr	Ry 
 *
 */

static 
void verify_call_arc(unsigned long *callee, unsigned long caller)
{
    union mips_instruction insn;
    int reg;
    __u32 addr = 0;
    __u32 pc;
    int offset;

    if (!is_kern_area(caller)) goto unexpected;

    /* skip jump delay slot */
    caller -= 2*INST_SIZE;
    if (get_kern_word((__u32 *)caller, (__u32 *)&insn)) 
	goto unexpected;

    /* check jal ADDR */
    if (insn.j_format.opcode == jal_op) {
	pc = (caller & 0xf0000000)|(insn.j_format.target << 2);
	if ( pc != *callee ){
	    goto changed;
	}
	return;
    } 

    /* check jalr Ry */
    if (insn.r_format.opcode == spec_op
	    && insn.r_format.func == jalr_op
	    && insn.r_format.rd == REG_RA
	    && insn.r_format.re == 0
	    && insn.r_format.rt == 0) {

	reg = insn.r_format.rs;

	/* skip nops */
	do {
	    caller -= INST_SIZE;
	    if ((!is_kern_area(caller))
		    || get_kern_word((__u32 *)caller, (__u32 *)&insn)) 
		goto unexpected;
	} while ( insn.word == 0 );

	/* lw Ry, %lo(xx)(Rx) */
	if (!(insn.i_format.opcode == lw_op
	      && insn.u_format.rt == reg)) {
	    goto unexpected;
	}
	offset = insn.i_format.simmediate;
	reg = insn.r_format.rs;

	/* skip nops */
	do {
	    caller -= INST_SIZE;
	    if ((!is_kern_area(caller))
		     || get_kern_word((__u32 *)caller, (__u32 *)&insn)) 
		goto unexpected;
	} while ( insn.word == 0 );

	/* lui Rx, %hi(xx) */
	if (!( insn.u_format.opcode == lui_op
			&& insn.u_format.rt == reg
			&& insn.u_format.rs == 0 )) {
		goto unexpected;
	}
	addr = (insn.u_format.uimmediate << 16);
	addr += offset;

	if (get_kern_word((__u32 *)addr, &pc) || !is_kern_area(pc))
	    goto unexpected;
	if ( pc != *callee )
	    goto changed;
	return;
    }
unexpected:
#ifdef DEBUG
    printk ("verify_call_arc:unexpceted caller.\n");
#endif
	return;
changed:
#ifdef DEBUG
    printk ( "verify_call_arc:change callee.(%lx -> %lx (* %lx))\n",
		*callee, (unsigned long)pc, (unsigned long)addr);
#endif
    *callee = pc;
    return;
}


/* return zero if invalid */
static unsigned long
heuristic_find_proc_start (unsigned long pc)
{
    union mips_instruction insn;
    int found = 0;


    while (is_kern_area(pc)) {
	__u32 new_insn;
	if (get_kern_word((__u32 *)pc, &new_insn))
	    break;

	insn.word = new_insn;
	if ((insn.r_format.opcode == spec_op 
		&& insn.r_format.func == jr_op 
		&& insn.r_format.rs == REG_RA 
		&& insn.r_format.rd == 0 
		&& insn.r_format.re == 0 
		&& insn.r_format.rt == 0)	/* jr ra */
	      || (insn.r_format.opcode == spec_op 
		&& insn.r_format.func == jr_op 
		&& insn.r_format.rs == REG_K0 
		&& insn.r_format.rd == 0 
		&& insn.r_format.re == 0 
		&& insn.r_format.rt == 0)	/* jr k0 */
	      || (insn.r_format.opcode == spec_op 
		&& insn.r_format.func == jr_op 
		&& insn.r_format.rs == REG_K1 
		&& insn.r_format.rd == 0 
		&& insn.r_format.re == 0 
		&& insn.r_format.rt == 0)	/* jr k1 */
	      || (insn.r_format.opcode == cop0_op 
		&& insn.r_format.func == eret_op 
		&& insn.r_format.rs ==  cop_op
		&& insn.r_format.rd == 0 
		&& insn.r_format.re == 0 
		&& insn.r_format.rt == 0)	/* eret */
	      || (insn.u_format.opcode == beq_op 
		&& insn.u_format.rs ==  0
		&& insn.u_format.rt == 0
		&& insn.u_format.uimmediate == 0xffff))	
					/* beq zero,zero, self */
	{
	    found = 1;
	    break;
	}
	pc -= INST_SIZE;
    }

    if (!found) {
	return 0;
    }

    /* skip RET-insn itself and BD slot */
    pc += 2 * INST_SIZE;

    /* skip nops */
    while (1) {
	if ((!is_kern_area(pc)) 
	    || get_kern_word((__u32 *)pc, (__u32 *)&insn)) 
	        return 0; /* can't be occured. */
        if ( insn.word != 0 ) 
    	    break;
        pc += INST_SIZE;
    }

    return pc;
}


/* return -1 as frame_size, if frame is invalid */
static void
get_frame_info (unsigned long pc_start, unsigned long pc,
		unsigned long *ra, unsigned long *sp, 
		unsigned long *fp, int *frame_size)
{
    union mips_instruction insn;
    char *errmsg;
    unsigned long epc;
    int ra_offset = 0;
    int fp_offset = 0;
    int sp_frame_size = 0;
    int fp_frame_size = 0;
    int frame_ptr = REG_SP;

    epc  = pc_start;

    while  (epc<pc) {
        __u32 new_insn;
	if (get_kern_word((__u32 *)epc, &new_insn))
	    break;
	insn.word = new_insn;
	if ((insn.i_format.opcode == addi_op 
	     || insn.i_format.opcode == addiu_op )
	    && insn.i_format.rs == REG_SP 
	    && insn.i_format.rt == REG_SP) {

	    /* addui sp,sp,-i, addi sp,sp, -i */
	    if (insn.i_format.simmediate > 0) break;
	    sp_frame_size += -insn.i_format.simmediate;

	} else if (insn.i_format.opcode == sw_op 
		   && insn.i_format.rs == REG_SP 
		   && insn.i_format.rt == REG_RA) {

	    /* sw    ra,xx(sp) */
	    ra_offset = sp_frame_size - insn.i_format.simmediate;

	} else if (insn.i_format.opcode == sw_op 
		   && insn.i_format.rs == REG_FP 
		   && insn.i_format.rt == REG_RA) {

	    /* sw    ra,xx(fp) */
	    ra_offset = fp_frame_size - insn.i_format.simmediate;

	} else if (insn.i_format.opcode == sw_op 
		   && insn.i_format.rs == REG_SP 
		   && insn.i_format.rt == REG_FP) {

	    /* sw    fp,xx(sp) */
	    fp_offset = sp_frame_size - insn.i_format.simmediate;
	    
	} else if ((insn.r_format.opcode == spec_op 
		    && insn.r_format.func == addu_op 
		    && insn.r_format.rd == REG_FP 
		    && insn.r_format.rs == REG_SP 
		    && insn.r_format.re == 0 
		    && insn.r_format.rt == 0) 
		   || (insn.r_format.opcode == spec_op 
		       && insn.r_format.func == or_op 
		       && insn.r_format.rd == REG_FP 
		       && insn.r_format.rs == REG_SP 
		       && insn.r_format.re == 0 
		       && insn.r_format.rt == 0)
		   || (insn.r_format.opcode == spec_op 
		       && insn.r_format.func == daddu_op 
		       && insn.r_format.rd == REG_FP 
		       && insn.r_format.rs == REG_SP 
		       && insn.r_format.re == 0 
		       && insn.r_format.rt == 0)) {

	    /* move    fp, sp */
	    /* (addu fp,sp,zero, fp,sp,zero OR daddu fp,sp,zero) */
	    fp_frame_size = sp_frame_size;
	    frame_ptr = REG_FP;

	}
	epc += INST_SIZE;
    }

#ifdef DEBUG
    printk("===========\n");
    printk("ra_offset:%d\n", ra_offset);
    printk("fp_offset:%d\n", fp_offset);
    printk("sp_frame_size:%d\n", sp_frame_size);
    printk("fp_frame_size:%d\n", fp_frame_size);
    printk("frame_ptr:%d\n", frame_ptr);
    printk("sp:%lx\n", *sp);
    printk("fp:%lx\n", *fp);
#endif

    if (!sp_frame_size) {
	*frame_size = 0;
	goto out;
    }
	
    if (frame_ptr == REG_SP) {
	*sp =  *sp + sp_frame_size;
	*frame_size = sp_frame_size;
    } else {
 	*sp =  *fp + fp_frame_size;
	*frame_size = fp_frame_size;
    }
    if (!is_kern_area(*sp)) {
	errmsg = "sp";
	goto error;
    }
    if (!is_aligned (*frame_size)) {
    	errmsg = "frame_size";
	goto error;
    }

#ifdef DEBUG
    dump_stack(*sp);
#endif
    if (ra_offset) {
	__u32 new_ra;

	if (get_kern_word((__u32 *)(*sp - ra_offset), &new_ra)
	    || !is_kern_area(new_ra)) {
	    errmsg = "ra";
	    goto error;
	}
	*ra =  new_ra;
    }
    if (fp_offset) {
	__u32 new_fp;
	if (get_kern_word((__u32 *)(*sp - fp_offset), &new_fp)) {
	    errmsg = "fp";
	    goto error;
	}
	*fp =  new_fp;
    }

out:
#ifdef DEBUG
    printk("caller_fp:%lx\n", *fp);
    printk("caller_sp:%lx\n", *sp);
    printk("new_ra:%lx\n", *ra);
    printk("===========\n");
#endif
    return;
error:
#ifdef DEBUG
    printk("error:%s\n", errmsg);
    printk("===========\n");
#endif
    * frame_size = -1;
    return;
}

#ifdef DEBUG
static
void show_frame_size (unsigned long func, int frame_size)
{
    DECL_SYMBOL_STRING_SPACE(callee_name)

    INIT_SYMBOL_STRING_SPACE(callee_name)
    if (is_kern_area(func))
        sprint_symbol(callee_name, sizeof(callee_name)-1 ,func);
    printk("     frame size of [%8.8lx]:\"%s\": %d\n",
	   func, callee_name, frame_size);
}
#endif

static
void show_pc (unsigned long pc, unsigned long sp, unsigned long fp)
{
    DECL_SYMBOL_STRING_SPACE(name)

    INIT_SYMBOL_STRING_SPACE(name)
    if (is_kern_area(pc) )
	sprint_symbol(name, sizeof(name)-1, pc);

    printk("  [<%8.8lx>:%s]\n        sp:%8.8lx    fp:%8.8lx\n",
	   pc, name, sp, fp);
}

static
void show_trace_pc (unsigned long func,
		    unsigned long ra, unsigned long sp, unsigned long fp)
{
    DECL_SYMBOL_STRING_SPACE(callee_name)
    DECL_SYMBOL_STRING_SPACE(caller_name)


    ra -= 2*INST_SIZE;
    INIT_SYMBOL_STRING_SPACE(callee_name)
    if ( is_kern_area(func) )
		sprint_symbol(callee_name, sizeof(callee_name)-1, func);
    INIT_SYMBOL_STRING_SPACE(caller_name)
    if ( is_kern_area(ra) )
		sprint_symbol(caller_name, sizeof(caller_name)-1, ra);

    printk("  [<%8.8lx>:%s] called by [<%8.8lx>:%s]\n",
		func,
		callee_name,
		ra,
		caller_name);
    printk("        sp:%8.8lx    fp:%8.8lx\n", sp, fp);
}

static
void show_exception (struct pt_regs *regs, unsigned long callee)
{
	
    static const char *(exc_name[]) = {
		"Int", "mod", "TLBL", "TLBS",
		"AdEL", "AdES", "IBE", "DBE",
		"Syscall", "Bp", "RI", "CpU",
		"Ov", "TRAP", "VCEI", "FPE",
		"C2E", "(17)", "(18)", "(19)",
		"(20)", "(21)", "(22)", "Watch",
		"(24)", "(25)", "(26)", "(27)",
		"(28)", "(29)", "(30)", "VCED"};

    unsigned long pc = regs->cp0_epc;
    unsigned long ra = get_gpreg(regs, REG_RA);
    int exc_code = (regs->cp0_cause>>2) & 31;
    DECL_SYMBOL_STRING_SPACE (name)


    if (regs->cp0_cause>>31) {
	/* Exception is occured in BD slot. In this case, CP0_EPC points to
	 restart point, not to BD slot. */
	pc += INST_SIZE;
    }

    INIT_SYMBOL_STRING_SPACE (name)
    if ( is_kern_area(callee) )
	sprint_symbol(name, sizeof(name)-1 ,callee);
    printk("  [<%8.8lx>:%s] called by exception.\n",callee, name);

    INIT_SYMBOL_STRING_SPACE (name)
    if ( is_kern_area(pc) )
	sprint_symbol(name, sizeof(name)-1 ,pc);
    printk("    EPC   : %08lx:%s\n", pc, name);

    INIT_SYMBOL_STRING_SPACE (name)
    if ( is_kern_area(ra) )
	sprint_symbol(name, sizeof(name)-1 ,ra);
    printk("    RA    : %08lx:%s\n", ra, name);
    printk("    GP    : %08lx    Status: %08lx\n",
			get_gpreg(regs, REG_GP),
			regs->cp0_status);
    printk("    Cause : %08lx    ExcCode:%s(%d)\n", 
			regs->cp0_cause, exc_name[exc_code],exc_code);

}

void
show_stacktrace (struct pt_regs *regs)
{
    int frame_size;
    unsigned long current_p;
    unsigned long stack_lower_lim, stack_upper_lim;
    unsigned long pc, start_pc;
    unsigned long new_sp,new_ra,new_fp;

    if (regs) {
	current_p = get_gpreg(regs, REG_GP);
	new_ra = get_gpreg(regs, REG_RA);
	new_sp = get_gpreg(regs, REG_SP);
	new_fp = get_gpreg(regs, REG_FP);
	pc = (unsigned long)regs->cp0_epc;

    } else {
	current_p = (unsigned long) current;
	asm("\tmove %0,$sp" : "=r"(new_sp));
	asm("\tmove %0,$fp" : "=r"(new_fp));
	asm("\tmove %0,$31" : "=r"(new_ra));
	get_frame_info( (unsigned long)show_stacktrace,
			(unsigned long)&&here,
			&pc,
			&new_sp,
			&new_fp,
			&frame_size);
    here:
    }
    stack_lower_lim = get_stack_lower(current_p);
    stack_upper_lim = get_stack_upper(current_p);
retry:
    start_pc = heuristic_find_proc_start(pc);
    show_pc(pc, new_sp, new_fp);
    while ((is_kern_area(pc)) && frame_size >= 0 
	   && new_sp >= stack_lower_lim && new_sp <= stack_upper_lim) {
	get_frame_info (start_pc,
			pc,
			&new_ra,
			&new_sp,
			&new_fp,
			&frame_size);

	if (pc == new_ra) {
	    break;
	}

	verify_call_arc (&start_pc, new_ra);
	show_trace_pc (start_pc, new_ra, new_sp, new_fp);

#ifdef DEBUG
	printk ("func:%lx called from %lx(func:%lx) frame_size:%d\n", 
	       start_pc, 
	       new_ra,
	       heuristic_find_proc_start (new_ra),
	       frame_size);
#endif
	pc = new_ra;
	start_pc = heuristic_find_proc_start (pc);
    }
    
    if (frame_size == 0) {
        /* check stack */
	if ((new_sp + sizeof(struct pt_regs) - 1) > stack_upper_lim)
	    return;

	/* exception frame */
	regs = (struct pt_regs *)new_sp;
	current_p = get_gpreg (regs, REG_GP);
	stack_lower_lim = get_stack_lower(current_p);
	stack_upper_lim = get_stack_upper(current_p);

	show_exception(regs, start_pc);

	new_ra = get_gpreg (regs, REG_RA);
	new_sp = get_gpreg (regs, REG_SP);
	new_fp = get_gpreg (regs, REG_FP);
	pc = (unsigned long)regs->cp0_epc;
	if (regs->cp0_cause>>31) {
	    pc += INST_SIZE;
	}
	goto retry;
    }
}

void
show_stacktrace_self (void)
{
    show_stacktrace(0);
}

/*
 * Return saved PC of a blocked thread.
 */
unsigned long thread_saved_pc(struct thread_struct *t)
{
    extern void ret_from_sys_call(void);
    unsigned long ra, sp, fp, caller;
    int frame_size;

    ra = (unsigned long) t->reg31;
    /* New born processes are a special case */
    if (ra == (unsigned long) ret_from_sys_call)
	return ra;

    sp = (unsigned long) t->reg29;
    fp = (unsigned long) t->reg30;
    caller = ra;
    get_frame_info( (unsigned long) schedule,
		    caller,
		    &ra,
		    &sp,
		    &fp,
		    &frame_size);
    return ra;
}

/*
 * These bracket the sleeping functions..
 */
extern void scheduling_functions_start_here(void);
extern void scheduling_functions_end_here(void);
#define first_sched     ((unsigned long) scheduling_functions_start_here)
#define last_sched      ((unsigned long) scheduling_functions_end_here)

/*
 * Return caller of sleeping functions.
 */

unsigned long get_wchan(struct task_struct *p)
{
    extern void ret_from_sys_call(void);
    unsigned long pc, start_pc;
    unsigned long ra, sp, fp, caller;
    int frame_size;
    struct thread_struct *th;
    unsigned long current_p;
    unsigned long stack_lower_lim, stack_upper_lim;

    current_p = (unsigned long)p;
    stack_lower_lim = get_stack_lower(current_p);
    stack_upper_lim = get_stack_upper(current_p);
    th = &p->thread;

    ra = (unsigned long) th->reg31;
    /* New born processe is a special case */
    if (ra == (unsigned long) ret_from_sys_call)
        return ra;

    sp = (unsigned long) th->reg29;
    fp = (unsigned long) th->reg30;
    caller = ra;
    get_frame_info((unsigned long) schedule,
		   caller,
		   &ra,
		   &sp,
		   &fp,
		   &frame_size);

    pc = ra;
    while ((is_kern_area(pc)) && frame_size >= 0 
	   && sp >= stack_lower_lim && sp <= stack_upper_lim
	   && first_sched <= pc && pc < last_sched ) {
      
	start_pc = heuristic_find_proc_start(pc);
	get_frame_info (start_pc,
			pc,
			&ra,
			&sp,
			&fp,
			&frame_size);
	pc = ra;
    }
    if (first_sched <= pc && pc < last_sched ) {
	return caller;
    }
    return pc;
}
