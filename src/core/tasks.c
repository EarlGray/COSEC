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

#if TASK_DEBUG
#   define __DEBUG
#endif

#include <stdlib.h>

#include <tasks.h>

#include <arch/i386.h>
#include <dev/intrs.h>
#include <dev/timer.h>

#include <log.h>

task_struct default_task;
task_struct *current = &default_task;

task_next_f         task_next           = null;

inline static int task_sysinfo_size(task_struct *task) {
    return (task->tss.cs == SEL_KERN_CS) ? 3 : 5;
}

/***
  *     Task switching
 ***/

/* this routine is normally called from within interrupt! */
static void task_save_context(task_struct *task) {
    if (task == null) return;

    /* ss3:esp3, efl, cs:eip, general-purpose registers, DS and ES are saved */
    uint *context = (uint *)intr_context_esp();
    assertv(context, "task_save_context: intr_ret is cleared!");

    task->tss.esp0 = (uint)context + CONTEXT_SIZE + task_sysinfo_size(task)*sizeof(uint);

    logmsgdf("\n| save cntxt=%x |", (uint)context);
}

/* this routine is normally called from within interrupt! */
static void task_push_context(task_struct *task) {
    uint context = task->tss.esp0 - CONTEXT_SIZE - task_sysinfo_size(task)*sizeof(uint);
    intr_set_context_esp(context);

    logmsgdf(" push cntxt=%x |\n", context);
}

static inline void task_cpu_load(task_struct *task) {
    /* unmark current task as busy */
    segment_descriptor *tssd = i386_gdt() + task->tss_index;
    segdescr_taskstate_busy(*tssd, 0);

    /* load next task */
    segment_selector tss_sel = { .as.word = make_selector(task->tss_index, 0, PL_USER) };
    i386_load_task_reg( tss_sel );
}

static void task_timer_handler(uint tick) {
    if (task_next) {    // is there a scheduler
        task_struct *next = task_next(tick);
        if (next) {     // switch to the next task is needed
            task_save_context(current);
            task_push_context(next);

            task_cpu_load(next);
            current = next;
        }
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

    uint *context = stack - CONTEXT_SIZE/sizeof(uint);
    context[0] = tss->ds;
    context[1] = tss->es;
    context[2] = tss->fs;
    context[3] = tss->gs;

    /* register task TSS in GDT */
    segment_descriptor taskdescr;
    segdescr_taskstate_init(taskdescr, (uint)tss, PL_USER);

    task->tss_index = gdt_alloc_entry(taskdescr);
    assertv( task->tss_index, "Error: can't allocate GDT entry for TSSD\n");
    logmsgdf("new TSS <- GDT[%x]\n", task->tss_index);

    /* init is done */
    task->state = TS_READY;
}

inline void task_kthread_init(task_struct *ktask, void *entry, void *k_esp) {
    const segment_selector kcs = { .as.word = SEL_KERN_CS };
    const segment_selector kds = { .as.word = SEL_KERN_DS };
    task_init(ktask, entry, k_esp, k_esp, kcs, kds);
}


void tasks_setup(void) {
    // initialize default task
    segment_descriptor taskdescr;
    segdescr_taskstate_init(taskdescr, (uint)&default_task.tss, PL_KERN);
    default_task.tss_index = gdt_alloc_entry(taskdescr);
    default_task.ldt_index = GDT_DEFAULT_LDT;

    timer_push_ontimer(task_timer_handler);
}

