#include <syscall.h>
#include <arch/i386.h>

void sys_exit();

void int_syscall() {
    k_printf("\n#SYS\n");

    uint * stack = (uint *)intr_context_esp() + CONTEXT_SIZE/sizeof(uint);
    uint intr_num = *(stack - 1);
    uint arg1 = *(stack - 2);
    uint arg2 = *(stack - 3);
    uint arg3 = *(stack - 4);

    k_printf("syscall(%d, 0x%x, 0x%x, 0x%x))\n", 
            intr_num, arg1, arg2, arg3);
}

