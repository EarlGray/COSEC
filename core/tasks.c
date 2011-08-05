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

#include <dev/intrs.h>
#include <dev/timer.h>

#define TASK_VERBOSE_DEBUG  (0)

task_struct default_task;
task_struct *current = &default_task;

task_need_switch_f  task_need_switch    = null;
task_next_f         task_next           = null;

/***
  *     Task switching 
 ***/

/* this routine is normally called from within interrupt! */
static void task_save_context(task_struct *task) {
    if (task == null) return;

    uint *stack = intr_stack_ret_addr();
    if (!stack) {
        k_printf("task_save_context: intr_ret is cleared!");
        return;
    }
    /* only for interrupts/exceptions without errcode on stack! */
    i386_gp_regs *regs = (i386_gp_regs *)((uint8_t*)stack - sizeof(i386_gp_regs));

    task->tss.esp0 = (ptr_t)stack;
    task->tss.eip = stack[0];
    task->tss.cs = stack[1];
    task->tss.eflags = stack[2];
#if TASK_VERBOSE_DEBUG
    k_printf("\n|<%x>save %x:%x:%x<-%x |", (uint)task, stack[0], stack[1], stack[2], (uint)stack);
#endif

    task->tss.eax = regs->eax;      task->tss.ecx = regs->ecx;
    task->tss.edx = regs->edx;      task->tss.ebx = regs->ebx;
    task->tss.esp = regs->esp;      task->tss.ebp = regs->ebp;
    task->tss.esi = regs->esi;      task->tss.edi = regs->edi;
}

/* this routine is normally called from within interrupt! */
static void task_push_context(task_struct *task) {
    uint *stack = (uint *)intr_stack_ret_addr();
    if (!stack) {
        k_printf("task_push_context: intr_ret is cleared!");
        return;
    }
    uint *new_stack = (uint *)task->tss.esp0;
    i386_gp_regs *regs = (i386_gp_regs *)((uint8_t*)stack - sizeof(i386_gp_regs));

    new_stack[0] = stack[0] = task->tss.eip;
    new_stack[1] = stack[1] = task->tss.cs;
    new_stack[2] = stack[2] = task->tss.eflags;
#if TASK_VERBOSE_DEBUG
    k_printf("|<%x>push %x:%x:%x->%x |\n", 
                (uint)task, stack[0], stack[1], stack[2], (uint)new_stack);
#endif    
    regs->eax = task->tss.eax;      regs->ecx = task->tss.ecx;    
    regs->edx = task->tss.edx;      regs->ebx = task->tss.ebx;
    regs->esp = task->tss.esp;      regs->ebp = task->tss.ebp;
    regs->esi = task->tss.esi;      regs->edi = task->tss.edi;
}

static void task_timer_handler(uint tick) {
    if (task_need_switch && task_need_switch(tick)) {
        task_struct *next = task_next();

        task_save_context(current);
        task_push_context(next);

        current = next;
        k_printf(">");
    }
}

inline task_struct *task_current(void) {
    return current;
}

inline void task_set_scheduler(task_need_switch_f need_switch, task_next_f next) {
    task_need_switch = need_switch;
    task_next = next;
}

void task_init(task_struct *task, void *entry, void *stack_end) {
    tss_t *tss = &(task->tss);
    tss->eflags = x86_eflags();
    tss->esp0 = tss->esp = (ptr_t)stack_end;
    tss->eip = (uint)entry;
    tss->cs = SEL_KERN_CS;
    tss->ds = tss->es = tss->fs = tss->gs = SEL_KERN_DS;
    tss->ss0 = tss->ss = SEL_KERN_DS;
    tss->ldt = SEL_DEFAULT_LDT;

    task->state = TS_READY;
}

void tasks_setup(void) {
    timer_set_frequency(TASK_DEFAULT_QUANT);
    timer_push_ontimer(task_timer_handler);
    irq_mask(true, 0);
}

