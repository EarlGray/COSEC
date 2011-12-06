#include <syscall.h>
#include <arch/i386.h>

#include <log.h>

uint sys_exit();
uint sys_print(const char **fmt);

typedef uint (*syscall_handler)();

#define SYSCALL_MAX 255
const syscall_handler syscalls[] = {

    [SYSCALL_MAX] = sys_print,
};

#define return_with_msg_if_not(condition, msg)  \
    do { if (! (condition) ) {   k_printf(msg); return ; } }  while 0

void int_syscall() {
    uint * stack = (uint *)intr_context_esp() + CONTEXT_SIZE/sizeof(uint);
    uint intr_num = *(stack - 1);
    uint arg1 = *(stack - 2);
    uint arg2 = *(stack - 3);
    uint arg3 = *(stack - 4);

    logf("\n#syscall(%d, 0x%x, 0x%x, 0x%x)\n", 
            intr_num, arg1, arg2, arg3);

    if (intr_num > SYSCALL_MAX) {
        logf("#SYS: invalid syscall 0x%x\n", intr_num);
        return;
    }

    const syscall_handler callee = syscalls[intr_num];
    if (callee == null) {
        logf("#SYS: invalid handler for syscall[0x%x]\n", intr_num);
        return;
    }

    callee(arg1, arg2, arg3);
}

uint sys_print(const char **fmt) {
    print(*fmt);
    return 0;
}
