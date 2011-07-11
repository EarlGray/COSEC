/* 
 *  This file will be a temporary task polygon
 */ 

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

int multitasking_setup(void) {
}
