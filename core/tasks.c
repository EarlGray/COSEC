/* 
 *  This file will is a temporary task training ground
 *
 *      Note on terminology:
 *  Stack during an interrupt:
 * -task -> <--- cpu ----> <--- intr data --
 *    -----|----|----|----|-------
 *           flg  cs  eip  
 *                        ^
 *          point of support for the interrupt
 */
#include <tasks.h>

#include <arch/i386.h>
#include <dev/intrs.h>
#include <dev/timer.h>

#define TASK_VERBOSE_DEBUG  (1)

task_struct default_task;
task_struct *current = &default_task;

task_need_switch_f  task_need_switch    = null;
task_next_f         task_next           = null;

/***
  *     Task switching 
 ***/

#define return_if_not(assertion, msg)   \
    if (!(assertion)) { k_printf(msg); return; }

/* this routine is normally called from within interrupt! */
static void task_save_context(task_struct *task) {
    if (task == null) return;

    uint *stack = intr_stack_ret_addr();
    return_if_not (stack, "task_save_context: intr_ret is cleared!");

    /* only for interrupts/exceptions without errcode on stack! */
    i386_gp_regs *regs = (i386_gp_regs*)((uint8_t*)stack - sizeof(i386_gp_regs));

    task->tss.eax = regs->eax;      task->tss.ecx = regs->ecx;
    task->tss.edx = regs->edx;      task->tss.ebx = regs->ebx;
    /*task->tss.esp = regs->esp0;*/ task->tss.ebp = regs->ebp;
    task->tss.esi = regs->esi;      task->tss.edi = regs->edi;

    task->tss.esp0 = (ptr_t)stack;
    task->tss.eip = stack[0];
    task->tss.cs = stack[1];
    task->tss.eflags = stack[2];
    if (task->tss.cs != SEL_KERN_CS) {
        /* %esp becomes a userspace pointer immediately after iret, 
           so a little hack for kernel stack is needed */
        task->tss.esp0 += 5 * sizeof(uint);   

        task->tss.esp = stack[3];
        task->tss.ss = stack[4];
    }

#if TASK_VERBOSE_DEBUG
    k_printf("\n|<%x>%x:%x:%x<-%x|", task->tss_index, stack[0], stack[1], stack[2], (uint)stack);
    k_printf("|s%x:%x|", stack[4], stack[3]);
#endif
}

/* this routine is normally called from within interrupt! */
static void task_push_context(task_struct *task) {
    uint *stack = (uint *)intr_stack_ret_addr();
    return_if_not (stack, "task_save_context: intr_ret is cleared!");

    uint *new_stack = (uint *)task->tss.esp0;
    i386_gp_regs *regs = (i386_gp_regs *)((uint8_t *)stack - sizeof(i386_gp_regs));

    new_stack[0] = task->tss.eip;   
    new_stack[1] = task->tss.cs;    
    new_stack[2] = task->tss.eflags;
    if (task->tss.cs != SEL_KERN_CS) {
        new_stack[3] = task->tss.esp;
        new_stack[4] = task->tss.ss;
    }

    regs->eax = task->tss.eax;      regs->ecx = task->tss.ecx;    
    regs->edx = task->tss.edx;      regs->ebx = task->tss.ebx;
    regs->esp = (uint)new_stack;    regs->ebp = task->tss.ebp;
    regs->esi = task->tss.esi;      regs->edi = task->tss.edi;

#if TASK_VERBOSE_DEBUG
    k_printf("|<%x>%x:%x:%x->%x|\n", 
                task->tss_index, new_stack[0], new_stack[1], 
                new_stack[2], (uint)new_stack);
    k_printf("|p%x:%x|", new_stack[4], new_stack[3]);
#endif    
}

static inline void task_cpu_load(task_struct *task) {
    /* unmark current task as busy */
    segment_descriptor *tssd = i386_gdt() + task->tss_index;
    segdescr_taskstate_busy(*tssd, 0);

    /* load next task */
    segment_selector tss_sel = { .as.word = make_selector(task->tss_index, 0, PL_USER) };
    asm ("ltrw %%ax     \n\t"::"a"( tss_sel ));
}

static void task_timer_handler(uint tick) {
    if (task_need_switch && task_need_switch(tick)) {
        task_struct *next = task_next();

        task_save_context(current);
        task_push_context(next);

        task_cpu_load(next);
#if TASK_VERBOSE_DEBUG
        k_printf(">");
#endif
        current = next;
    }
}

inline task_struct *task_current(void) {
    return current;
}

inline void task_set_scheduler(task_need_switch_f need_switch, task_next_f next) {
    task_need_switch = need_switch;
    task_next = next;
}

void task_init(task_struct *task, void *entry, 
        void *esp0, void *esp3, segment_selector cs, segment_selector ds) {
    tss_t *tss = &(task->tss);

    /* initial state */
    tss->esp0 = (ptr_t)esp0;
    tss->ss0 = SEL_KERN_DS;
    tss->esp = (ptr_t)esp3;
    tss->ss = ds.as.word;

    tss->cs = cs.as.word;
    tss->ds = tss->es = tss->fs = tss->gs = ds.as.word;

    tss->ldt = SEL_DEFAULT_LDT;
    tss->eflags = x86_eflags();
    tss->eip = (uint)entry;

    /* initialize stack as if the task was interrupted */
    uint *stack = (uint *)tss->esp0;
    stack[0] = tss->eip;
    stack[1] = tss->cs;
    stack[2] = tss->eflags;
    if (tss->cs != SEL_KERN_CS) {
        stack[3] = tss->esp;
        stack[4] = tss->ss;
    }

    /* register task TSS in GDT */
    segment_descriptor taskdescr;
    segdescr_taskstate_init(taskdescr, (uint)tss, PL_USER);

    task->tss_index = gdt_alloc_entry(taskdescr);
    return_if_not( task->tss_index, "Error: can't allocate GDT entry for TSSD\n");
#if TASK_VERBOSE_DEBUG
    k_printf("new TSS <- GDT[%x]\n", task->tss_index);
#endif

    /* init is done */
    task->state = TS_READY;
}

inline void task_kthread_init(task_struct *ktask, void *entry, void *k_esp) {
    const segment_selector kcs = { .as.word = SEL_KERN_CS };
    const segment_selector kds = { .as.word = SEL_KERN_DS };
    task_init(ktask, entry, k_esp, k_esp, kcs, kds);
}


void tasks_setup(void) {
    timer_set_frequency(TASK_DEFAULT_QUANT);
    timer_push_ontimer(task_timer_handler);
    irq_mask(0, true);
}

