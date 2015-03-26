#include <syscall.h>
#include <arch/i386.h>
#include <fs/fs_sys.h>

#include <log.h>

int sys_print(const char **fmt);

typedef int (*syscall_handler)();

const syscall_handler syscalls[] = {
    [SYS_MOUNT] = sys_mount,

    [SYS_MKDIR] = sys_mkdir,
    [SYS_LSDIR] = sys_lsdir,

    /*
    [SYS_PREAD] = sys_pread,
    [SYS_PWRITE] = sys_pwrite,
    */

    [SYS_PRINT] = sys_print,
};

void int_syscall() {
    uint * stack = (uint *)intr_context_esp() + CONTEXT_SIZE/sizeof(uint);
    uint intr_num = *(stack - 1);
    uint arg1 = *(stack - 2);
    uint arg2 = *(stack - 3);
    uint arg3 = *(stack - 4);

    logmsgdf("\n#syscall(%d, 0x%x, 0x%x, 0x%x)\n",
            intr_num, arg1, arg2, arg3);

    assertv( intr_num < N_SYSCALLS,
            "#SYS: invalid syscall 0x%x\n", intr_num);

    const syscall_handler callee = syscalls[intr_num];
    assertv(callee, "#SYS: invalid handler for syscall[0x%x]\n", intr_num);

    logmsgdf("callee *%x will be called...\n", (uint)callee);
    callee(arg1, arg2, arg3);
}

int sys_print(const char **fmt) {
    logmsgd("sys_printf()");
    k_printf(*fmt);
    return 0;
}
