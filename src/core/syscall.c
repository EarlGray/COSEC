#include <cosec/log.h>
#include <cosec/syscall.h>

#include "syscall.h"
#include "process.h"
#include "tasks.h"

#include "arch/intr.h"


int sys_print(const char **fmt) {
    process_t *proc = current_proc();

    /* TODO: make userspace stack into va_list somehow */
    logmsgf("[PID %d] %s", proc->ps_pid, *fmt);

    return 0;
}

int _sys_exit(int status) {
    sys_exit(status);
    return 0;
}


typedef int (*syscall_handler)();

const syscall_handler syscalls[] = {
    [SYS_EXIT]      = sys_exit,
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

void int_syscall() {
    struct interrupt_context *ctx = intr_context_esp();
    uint32_t intr_num = ctx->eax;
    uint32_t arg1 = ctx->ecx;
    uint32_t arg2 = ctx->edx;
    uint32_t arg3 = ctx->ebx;

    logmsgdf("%s: syscall(%d, 0x%x, 0x%x, 0x%x)\n",
            __func__, intr_num, arg1, arg2, arg3);

    // TODO: kill the process on unavailable syscall
    assertv(intr_num < N_SYSCALLS,
            "%s: invalid syscall 0x%x\n", __func__, intr_num);

    const syscall_handler callee = syscalls[intr_num];
    if (!callee) {
        logmsgif("%s: invalid handler for syscall[0x%x]\n", __func__, intr_num);
        // TODO: kill the process on unavailable syscall
        return;
    }

    uint32_t result = callee(arg1, arg2, arg3);
    logmsgdf("%s: syscall result = %d\n", __func__, result);
    ctx->eax = result;
}
