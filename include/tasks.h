#ifndef __TASKS_H__
#define __TASKS_H__

#include <arch/i386.h>

#define TS_RUNNING    0
#define TS_READY      1
#define TS_STOPPED    2

#define TASK_KERNSTACK_SIZE   0x800
#define TASK_DEFAULT_QUANT    0x1000

typedef int task_state_t;

struct task {
    tss_t tss;
    uint32_t tss_index;
    uint32_t ldt_index;
    task_state_t state;
};

typedef  struct task  task_struct;

/* thread function which will be executed */
typedef void (*task_f)(int, ...);
/* takes tick count from timer, return an obvious boolean value */
typedef bool (*task_need_switch_f)(uint);
/* returns next task */
typedef task_struct* (*task_next_f)(void);

task_struct *task_current(void);

void task_set_scheduler(task_need_switch_f need_switch, task_next_f next);

void task_init(task_struct *task, void *entry, void *stack_end);
    
void tasks_setup(void);

#endif // __TASKS_H__
