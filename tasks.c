/* 
 *  This file will is a temporary task training ground
 */ 
#include <arch/i386.h>

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
    for (;;) k_printf("B");
}

void do_task1(void) {
}

void do_task2(void) {
    for (;;) k_printf("A");
}

void sched(void) {
}

void multitasking_setup(void) {
}
