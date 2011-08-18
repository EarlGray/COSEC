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

#define CONTEXT_SIZE        0x30

task_struct default_task;
task_struct *current = &default_task;

task_next_f         task_next           = null;

/***
  *     Task switching 
 ***/

#define return_if_not(assertion, msg)   \
    if (!(assertion)) { k_printf(msg); return; }

/* this routine is normally called from within interrupt! */
static void task_save_context(task_struct *task) {
    if (task == null) return;

    /* ss3:esp3, efl, cs:eip, general-purpose registers, DS and ES are saved */
    uint *context = (uint *)intr_context_esp();
    return_if_not (context, "task_save_context: intr_ret is cleared!");

    task->tss.esp0 = (uint)context + CONTEXT_SIZE + 5*sizeof(uint);

#if TASK_DEBUG
    k_printf("\n| save cntxt=%x |", (uint)context);
#endif
}

/* this routine is normally called from within interrupt! */
static void task_push_context(task_struct *task) {
    uint context = task->tss.esp0 - CONTEXT_SIZE - 5*sizeof(uint);
    intr_set_context_esp(context);

#if TASK_DEBUG
    k_printf(" push cntxt=%x |\n", context);
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
    task_struct *next = 0;
    if (task_next) 
        if ((next = task_next(tick))) {
            task_save_context(current);
            task_push_context(next);

            task_cpu_load(next);
            current = next;
        }
}

inline task_struct *task_current(void) {
    return current;
}

inline void task_set_scheduler(task_next_f next) {
    task_next = next;
}

void task_init(task_struct *task, void *entry, 
        void *esp0, void *esp3, 
        segment_selector cs, segment_selector ds) 
{
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
    uint *stack = (uint *)(tss->esp0 - 5*sizeof(uint));
    stack[0] = tss->eip;
    stack[1] = tss->cs;
    stack[2] = tss->eflags;
    if (tss->cs != SEL_KERN_CS) {
        stack[3] = tss->esp;
        stack[4] = tss->ss;
    }

    uint *context = (uint *)((uint8_t *)stack - CONTEXT_SIZE);
    context[0] = tss->gs;
    context[1] = tss->fs;
    context[2] = tss->es;
    context[3] = tss->ds;

    /* register task TSS in GDT */
    segment_descriptor taskdescr;
    segdescr_taskstate_init(taskdescr, (uint)tss, PL_USER);

    task->tss_index = gdt_alloc_entry(taskdescr);
    return_if_not( task->tss_index, "Error: can't allocate GDT entry for TSSD\n");
#if TASK_DEBUG
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

