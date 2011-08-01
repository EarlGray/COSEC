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
#include <arch/i386.h>
#include <dev/intrs.h>

#define TASK_RUNNING    0
#define TASK_READY      1
#define TASK_STOPPED    2

#define TS_KERNSTACK_SIZE   0x800

typedef void (*task_f)(int, ...);
typedef int task_state_t;

struct task {
    tss_t tss;
    uint32_t tss_index;
    uint32_t ldt_index;
    task_state_t state;
};
typedef  struct task  task_struct;

void do_task0(void);
uint8_t task0_stack[TS_KERNSTACK_SIZE];

const task_struct task0 = {
    .tss = {
        .prev_task_link = 0,
        .esp0 = (ptr_t)task0_stack,
        .ss0 = SEL_KERN_DS,
        .eip = (ptr_t)do_task0,
        .cs = SEL_USER_CS,
        .ds = SEL_USER_DS,
        .es = SEL_USER_DS,
        .fs = SEL_USER_DS,
        .gs = SEL_USER_DS,
        .ss = SEL_USER_DS,
        .ldt = SEL_DEFAULT_LDT,
    },
    .state = TASK_RUNNING,
};

void do_task0(void) {
    for (;;) k_printf("0");
}

void do_task1(void) {
    for (;;) k_printf("1");
}

/* this routine is normally called from within interrupt! */
void task_save_context(task_struct *task) {
    uint *stack = intr_stack_ret_addr();
    /* only for interrupts/exceptions without errcode on stack! */
    i386_gp_regs *regs = (i386_gp_regs *)((uint8_t*)stack - sizeof(i386_gp_regs));

    task->tss.esp0 = (ptr_t)(stack + 3);
    task->tss.eip = stack[0];
    task->tss.cs = stack[1];
    task->tss.eflags = stack[2];

    task->tss.eax = regs->eax;      task->tss.ecx = regs->ecx;
    task->tss.edx = regs->edx;      task->tss.ebx = regs->ebx;
    task->tss.esp = regs->esp;      task->tss.ebp = regs->ebp;
    task->tss.esi = regs->esi;      task->tss.edi = regs->edi;
}

/* this routine is normally called from within interrupt! */
void task_push_context(task_struct *task) {
    uint *stack = (uint*)task->tss.esp0 - 3;
    i386_gp_regs *regs = (i386_gp_regs *)((uint8_t*)stack - sizeof(i386_gp_regs));

    stack[0] = task->tss.eip;
    stack[1] = task->tss.cs;
    stack[2] = task->tss.eflags;
    
    regs->eax = task->tss.eax;      regs->ecx = task->tss.ecx;    
    regs->edx = task->tss.edx;      regs->ebx = task->tss.ebx;
    regs->esp = task->tss.esp;      regs->ebx = tabp->tss.ebp;
    regs->edx = task->tss.esi;      regs->ebx = task->tss.edi;
}

static void copy_interrupt_data(
        task_struct *dest_stack_task, 
        uint *intr_stack_point_of_support) 
{
    uint *dst_stack = (uint *)dest_stack_task->tss.esp0 - 3;
    dst_stack[0] = intr_stack_point_of_support[0];
    dst_stack[1] = intr_stack_point_of_support[1];
    dst_stack[2] = intr_stack_point_of_support[2];
}

bool switch_context(uint tick) {
    return true;
}

task_t *current;

inline task_t *task_current(void) {
    return current;
}

task_t *next_task(void) {
    if (current == task0) return task1;
    return task0; 
}

void task_timer_handler(uint tick) {
    if (switch_context(tick)) {
        task_t *next = next_task();
        task_save_context(task_current());
        task_push_context(next);
        copy_interrupt_data(next, intr_stack_ret_addr());
    }
}

void multitasking_setup(void) {
    timer_t task_timer = timer_push_ontimer(task_timer_handler);
    
    timer_pop_ontimer(task_timer);
}
