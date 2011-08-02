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
#include <dev/timer.h>

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
void do_task1(void);
uint8_t task0_stack[TS_KERNSTACK_SIZE];
uint8_t task1_stack[TS_KERNSTACK_SIZE];

task_struct task0 = {
    .tss = {
        .prev_task_link = 0,
        .esp0 = (ptr_t)task0_stack + TS_KERNSTACK_SIZE - 0x20,
        .esp = (ptr_t)task0_stack + TS_KERNSTACK_SIZE - 0x20,
        .eip = (ptr_t)do_task0,
        .cs = SEL_KERN_CS,
        .ds = SEL_KERN_DS,
        .es = SEL_KERN_DS,
        .fs = SEL_KERN_DS,
        .gs = SEL_KERN_DS,
        .ss = SEL_KERN_DS,
        .ss0 = SEL_KERN_DS,
        .ldt = SEL_DEFAULT_LDT,
    },
    .state = TASK_RUNNING,
};

task_struct task1 = {
    .tss = {
        .prev_task_link = 0,
        .esp0 = (ptr_t)task1_stack + TS_KERNSTACK_SIZE - 0x20,
        .esp = (ptr_t)task1_stack + TS_KERNSTACK_SIZE - 0x20,
        .eip = (ptr_t)do_task1,
        .cs = SEL_KERN_CS,
        .ds = SEL_KERN_DS,
        .es = SEL_KERN_DS,
        .fs = SEL_KERN_DS,
        .gs = SEL_KERN_DS,
        .ss = SEL_KERN_DS,
        .ss0 = SEL_KERN_DS,
        .ldt = SEL_DEFAULT_LDT,
    },
    .state = TASK_RUNNING,
};

task_struct ext_task;

volatile task_struct *current;


#include <dev/kbd.h>

volatile bool quit = false;
void key_press(scancode_t scan) {
    //if (scan == 1) 
    k_printf("\n\nexiting...\n");
    quit = true;
}

void do_task0(void) {
    int i = 0;
    while (!quit) {
        int a = 1;
        for (a = 0; a < 1000000; ++a);
        if (0 == (i % 75)) {
            i = 0;
            k_printf("\r");
        }
        ++i;
        k_printf("0");
    }
}

void do_task1(void) {
    int i;
    while (!quit) {
       int a = 1;
       for (a = 0; a < 1000000; ++a);
       if (0 == (i % 75)) {
           i = 0;
           k_printf("\r");
       }
       ++i;
       k_printf("1");
    }
 }

/* this routine is normally called from within interrupt! */
void task_save_context(task_struct *task) {
    if (task == null) return;

    uint *stack = intr_stack_ret_addr();
    /* only for interrupts/exceptions without errcode on stack! */
    i386_gp_regs *regs = (i386_gp_regs *)((uint8_t*)stack - sizeof(i386_gp_regs));

    task->tss.esp0 = (ptr_t)(stack + 3);
    task->tss.eip = stack[0];
    task->tss.cs = stack[1];
    task->tss.eflags = stack[2];
    k_printf("| saved: %x:%x:%x from %x |\n", stack[0], stack[1], stack[2], (uint)stack);

    task->tss.eax = regs->eax;      task->tss.ecx = regs->ecx;
    task->tss.edx = regs->edx;      task->tss.ebx = regs->ebx;
    task->tss.esp = regs->esp;      task->tss.ebp = regs->ebp;
    task->tss.esi = regs->esi;      task->tss.edi = regs->edi;
}

/* this routine is normally called from within interrupt! */
void task_push_context(task_struct *task) {
    uint *stack = (uint *)intr_stack_ret_addr();
    uint *new_stack = (uint *)((uint8_t*)task->tss.esp0 - 3);
    i386_gp_regs *regs = (i386_gp_regs *)((uint8_t*)stack - sizeof(i386_gp_regs));

    new_stack[0] = stack[0] = task->tss.eip;
    new_stack[1] = stack[1] = task->tss.cs;
    new_stack[2] = stack[2] = task->tss.eflags;
    k_printf("| pushed: %x:%x:%x to %x |\n", stack[0], stack[1], stack[2], (uint)new_stack);
    
    regs->eax = task->tss.eax;      regs->ecx = task->tss.ecx;    
    regs->edx = task->tss.edx;      regs->ebx = task->tss.ebx;
    regs->esp = task->tss.esp;      regs->ebx = task->tss.ebp;
    regs->edx = task->tss.esi;      regs->ebx = task->tss.edi;
}

bool switch_context(uint tick) {
    return ! (tick % 20);
}


inline task_struct *task_current(void) {
    return current;
}

task_struct *next_task(void) {
    if (quit) return &ext_task;
    if (current == &task0) return &task1;
    if (current == &task1) return &task0; 
    if (current == &ext_task) return &task0;
    panic("Invalid task");
}

void task_timer_handler(uint tick) {
    if (! switch_context(tick)) 
        return;

    task_struct *next = next_task();

    task_save_context(task_current());
    task_push_context(next);

    current = next;
}

void tasks_setup(void) {
    uint eflags = 0;
    i386_eflags(eflags);
    task0.tss.eflags = task1.tss.eflags = eflags;

    current = &ext_task;
    kbd_set_onpress(key_press); 
    timer_t task_timer = timer_push_ontimer(task_timer_handler);

    do cpu_halt(); 
    while (!quit);
    cpu_halt();
    
    timer_pop_ontimer(task_timer);
    kbd_set_onpress(null);
}
