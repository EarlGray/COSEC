#ifndef __TASKS_H__
#define __TASKS_H__

#include <arch/i386.h>

#define TASK_KERNSTACK_SIZE   0x800

enum taskstate {
    TS_READY    = 0,
    TS_BLOCKED  = 1,
    TS_STOPPED  = 2,
    TS_EXITED   = 3,
};

struct task {
    tss_t           tss;
    uint32_t        tss_index;
    uint32_t        ldt_index;

    enum taskstate  state;
    struct task *   next;

    void *          kstack;
    size_t          kstack_size;

    void *          entry;
};

typedef  struct task  task_struct;

/* thread function which will be executed */
typedef void (*task_f)(int, ...);
/* returns next task or null if no context switch */
typedef task_struct* (*task_next_f)(uint tick);

task_struct *task_current(void);

void task_set_scheduler(task_next_f next);

void task_kthread_init(task_struct *ktask, void *entry, void *k_esp);

void task_init(task_struct *task, void *entry, 
        void *esp0, void *esp3,
        segment_selector cs, segment_selector ds
);

void tasks_setup(task_struct *default_task);

int sched_add_task(task_struct *task);

void task_yield(task_struct *task);

#endif // __TASKS_H__
