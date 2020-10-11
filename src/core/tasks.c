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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <cosec/log.h>

#include "arch/i386.h"
#include "arch/intr.h"
#include "dev/intrs.h"
#include "dev/timer.h"
#include "mem/paging.h"

#include "tasks.h"


task_struct *volatile theCurrentTask = NULL;

task_next_f         task_next           = null;

inline static int task_sysinfo_size(task_struct *task) {
    return (task->tss.cs == SEL_KERN_CS) ? 3 : 5;
}

/***
  *     Task switching
 ***/

static void task_save_context(task_struct *task) {
    if (task == null) return;

    /* ss3:esp3, efl, cs:eip, general-purpose registers, DS and ES are saved */
    struct interrupt_context *context = intr_context_esp();
    if (!context) {
        panic("task_save_context() called outside of a IRQ");
    }

    task->tss.esp0 = (uintptr_t)context + CONTEXT_SIZE + task_sysinfo_size(task)*sizeof(uint);

    logmsgdf("%s: ctx=*%x, tss=%d\n", __func__, context, task->tss_index);
}

/* this routine is normally called from within interrupt! */
static void task_push_context(task_struct *task) {
    uint32_t *esp0 = (uint32_t *)task->tss.esp0;
    uint32_t *iret_stack = esp0 - task_sysinfo_size(task);
    iret_stack[2] |= EFL_IF;

    void *context = (void*)iret_stack - CONTEXT_SIZE;
    intr_set_context_esp((uintptr_t)context);

    logmsgdf("%s: ctx=*%x, tss=%d\n", __func__, context, task->tss_index);

    if (iret_stack[1] == SEL_KERN_CS) {
        logmsgdf("%s: stack: efl=0x%x, cs:eip=%x:%x\n",
                __func__, iret_stack[2], iret_stack[1], iret_stack[0]);
    } else {
        logmsgdf("%s: stack: ss:esp=%x:%x, efl=0x%x, cs:eip=%x:%x\n", __func__,
                iret_stack[4], iret_stack[3], iret_stack[2], iret_stack[1], iret_stack[0]);
    }
}

static inline void task_cpu_load(task_struct *task) {
    /* unmark current task as busy */
    segment_descriptor *tssd = i386_gdt() + task->tss_index;
    segdescr_taskstate_busy(*tssd, 0);

    /* load next task */
    uint16_t pl = ((task->tss.cs == SEL_KERN_CS) ? PL_KERN : PL_USER);
    segment_selector tasksel = make_segment_selector(task->tss_index, SEL_TI_GDT, pl);

    logmsgdf("%s: tasksel = 0x%x, eflags = 0x%x, cr3 = @%x\n",
            __func__, tasksel.as.word, task->tss.eflags, task->tss.cr3);
    logmsgdf("%s: tssd = %x %x, base = *%x\n",
            __func__, tssd->as.ints[0], tssd->as.ints[1],
            tssd->as.strct.base_l + (tssd->as.strct.base_m << 16) + (tssd->as.strct.base_h << 24)
    );

    i386_load_task_reg(tasksel);
    i386_switch_pagedir((void *)task->tss.cr3);
}

static void task_timer_handler(uint tick) {
    if (!task_next)
        return; // no scheduler set

    task_struct *next = task_next(tick);
    if (!next)
        return; // no next task, keep the current one

    // switch to the next task:
    logmsgdf("%s(tick=%d)\n", __func__, tick);

    task_struct *current = (task_struct*)theCurrentTask;    // make a non-volatile copy
    task_save_context(current);

    task_cpu_load(next);
    task_push_context(next);

    theCurrentTask = next;
}

inline task_struct *task_current(void) {
    return (task_struct *)theCurrentTask;
}

inline void task_set_scheduler(task_next_f next) {
    task_next = next;
}

void task_init(task_struct *task, void *entry,
        void *esp0, void *esp3,
        segment_selector cs, segment_selector ds)
{
    tss_t *tss = &(task->tss);
    memset(tss, 0, sizeof(tss_t));

    /* initial state */
    tss->esp0 = (uintptr_t)esp0;
    tss->ss0 = SEL_KERN_DS;
    tss->esp = (uintptr_t)esp3;
    tss->ss = ds.as.word;

    tss->cs = cs.as.word;
    tss->ds = tss->es = tss->fs = tss->gs = ds.as.word;

    tss->ldt = SEL_DEF_LDT;
    tss->eflags = EFL_RSRVD | EFL_IF;
    tss->eip = (uintptr_t)entry;
    tss->io_map_addr = 0x64;
    tss->io_map1 = 0xffffffff;
    tss->io_map2 = 0xffffffff;

    /* initialize stack as if the task was interrupted */
    uint32_t *stack = (uint32_t*)tss->esp0 - task_sysinfo_size(task);
    logmsgdf("%s(task=*%x): esp0=*%x\n", __func__, task, esp0);
    stack[0] = tss->eip;
    stack[1] = tss->cs;
    stack[2] = tss->eflags;
    if (tss->cs != SEL_KERN_CS) {
        stack[3] = tss->esp;
        stack[4] = tss->ss;
    }

    uint32_t *context = stack - CONTEXT_SIZE/sizeof(uint);
    context[0] = tss->gs;
    context[1] = tss->fs;
    context[2] = tss->es;
    context[3] = tss->ds;

    /* register task TSS in GDT */
    segment_descriptor taskdescr;
    segdescr_taskstate_init(taskdescr, (uintptr_t)tss, PL_KERN);

    task->tss_index = gdt_alloc_entry(taskdescr);
    assertv( task->tss_index, "Error: can't allocate GDT entry for TSSD\n");
    logmsgdf("%s(task=*%x): tss = GDT[%d]\n", __func__, task, task->tss_index);

    /* init is done */
    task->state = TS_READY;
}

inline void task_kthread_init(task_struct *ktask, void *entry, void *k_esp) {
    const segment_selector kcs = { .as.word = SEL_KERN_CS };
    const segment_selector kds = { .as.word = SEL_KERN_DS };
    // TODO: change esp3 to NULL
    task_init(ktask, entry, k_esp, k_esp, kcs, kds);
}


/*
 *  Scheduling
 */
task_struct* the_scheduler(uint32_t tick) {
    task_struct *current = (task_struct *)theCurrentTask;   // a non-volatile copy
    for (task_struct *c = current->next; c && (c != current); c = c->next) {
        if (c->state == TS_READY)
            return c;
    }
    return NULL;
}

int sched_add_task(task_struct *task) {
    task_struct *current = (task_struct *)theCurrentTask;   // a non-volatile copy
    task_struct *was_next = current->next;
    task->next = was_next;
    current->next = task;
    return 0;
}

void task_yield(task_struct *task) {
    logmsgdf("%s: task=*%x\n", __func__, task);
    task_timer_handler(0);
}

/*
 *  Setup
 */

void tasks_setup(task_struct *default_task) {
    // initialize default task
    if (!default_task->tss.eip)
        default_task->tss.eip = (uintptr_t)default_task->entry;

    logmsgdf("%s: tss.eip = *%x\n", __func__, default_task->tss.eip);

    // initialize scheduling:
    default_task->next = default_task;
    theCurrentTask = default_task;

    task_set_scheduler(the_scheduler);
    timer_push_ontimer(task_timer_handler);

    task_cpu_load(default_task);
    theCurrentTask = default_task;

    tss_t *tss = &default_task->tss;
    i386_iret(tss->eip, tss->cs, tss->eflags, tss->esp, tss->ss);

    //logmsgef("%s: unreachable", __func__);
}
