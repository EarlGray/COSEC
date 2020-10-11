#include "syscall.h"
#include "process.h"
#include "arch/intr.h"
#include "tasks.h"

#define __DEBUG
#include <cosec/log.h>


int sys_print(const char **fmt) {
    logmsgdf("sys_printf()");
    k_printf(*fmt);
    return 0;
}

int sys_exit(int status) {
    process_t *proc = (process_t *)task_current();
    logmsgdf("%s(%d), pid=%d\n", __func__, status, proc->ps_pid);

    proc->ps_task.state = TS_EXITED;
    proc->ps_task.tss.eax = status;

    task_yield((task_struct*)proc);

    // TODO: cleanup resources
    // TODO: send SIGCHLD
    // TODO: orphan children to PID1

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
