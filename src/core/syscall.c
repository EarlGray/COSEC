#include <syscall.h>
#include <arch/i386.h>
#include <process.h>

#include <cosec/fs.h>
#include <cosec/log.h>

int sys_print(const char **fmt);

typedef int (*syscall_handler)();

const syscall_handler syscalls[] = {
    [SYS_READ]      = sys_read,
    [SYS_WRITE]     = sys_write,

    [SYS_OPEN]      = sys_open,
    [SYS_CLOSE]     = sys_close,

    [SYS_MKDIR]     = sys_mkdir,
    [SYS_RENAME]    = sys_rename,

    [SYS_UNLINK]    = sys_unlink,

    [SYS_LSEEK]     = sys_lseek,
    [SYS_GETPID]    = sys_getpid,
    [SYS_MOUNT]     = sys_mount,
    [SYS_PRINT]     = sys_print,
};

uint32_t int_syscall(uint32_t *stack) {
    uint32_t intr_num = stack[-1];   /* eax : syscall number */
    uint32_t arg1 = stack[-2];       /* ecx : arg1 */
    uint32_t arg2 = stack[-3];       /* edx : arg2 */
    uint32_t arg3 = stack[-4];       /* ebx : arg3 */

    logmsgdf("\n#syscall(%d, 0x%x, 0x%x, 0x%x)\n",
            intr_num, arg1, arg2, arg3);

    assertv( intr_num < N_SYSCALLS,
            "#SYS: invalid syscall 0x%x\n", intr_num);

    const syscall_handler callee = syscalls[intr_num];
    assertv(callee, "#SYS: invalid handler for syscall[0x%x]\n", intr_num);

    logmsgdf("callee *%x will be called...\n", (uint)callee);
    uint32_t result = callee(arg1, arg2, arg3);
    logmsgdf("callee result = %d\n", result);
    return result;
}

int sys_print(const char **fmt) {
    logmsgdf("sys_printf()");
    k_printf(*fmt);
    return 0;
}

