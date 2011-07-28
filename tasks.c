/* 
 *  This file will be a temporary task polygon
 */ 
#define TASK_RUNNING    0
#define TASK_READY      1
#define TASK_STOPPED    2

typedef void (*task_f)(int, ...);
typedef int task_state_t;

struct task_state_seg {
    uint prev_task_link;    // high word is reserved

    uint esp0;
    uint ss0;           // high word is reserved
    uint esp1;
    uint ss1;           // high word is reserved
    uint esp2;
    uint ss2;           // high word is reserved

    uint cr3;
  
    uint eip;
    uint eflags;  

    uint eax, ecx, edx, ebx;
    uint esp, ebp, esi, edi;

    uint es, cs, ss, ds, fs, gs;
    uint ldt;
    uint io_map_addr;
};
typedef  struct task_state_seg  tss_t;

struct task {
    tss_t tss;
    uint32_t tss_index;
    uint32_t ldt_index;
    task_state_t state;
};


void do_task1(void) {
    k_printf("B");
}

void do_task2(void) {
    k_printf("A");
}

void task_add(task_f task) {
    //
}

int multitasking_setup(void) {
    task_add(do_task1);
    task_add(do_task2);
}
