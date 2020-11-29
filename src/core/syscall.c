#include <sys/errno.h>
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

pid_t sys_waitpid(pid_t pid, int *wstatus, int flags) {
    logmsgef("%s: TODO", __func__);
    return -ECHILD;
}

int sys_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) {
    logmsgef("%s: TODO", __func__);
    return -ETODO;
}

int sys_setsid(void) {
    logmsgef("%s: TODO", __func__);
    return -ETODO;
}


typedef int (*syscall_handler)();

const syscall_handler syscalls[] = {
    [SYS_exit]      = _sys_exit,
    [SYS_read]      = sys_read,
    [SYS_write]     = sys_write,

    [SYS_open]      = sys_open,
    [SYS_close]     = sys_close,

    [SYS_waitpid]   = sys_waitpid,

    [SYS_mkdir]     = sys_mkdir,
    [SYS_rename]    = sys_rename,

    [SYS_unlink]    = sys_unlink,
    //[SYs_execve]    = sys_execve,

    [SYS_lseek]     = sys_lseek,
    [SYS_getpid]    = sys_getpid,
    [SYS_setsid]    = sys_setsid,
    [SYS_mount]     = sys_mount,

    [SYS_brk]       = (syscall_handler)sys_brk,

    [SYS_sigaction] = sys_sigaction,
    [SYS_fstat]     = sys_fstat,

    [SYS_print]     = sys_print,
};

void int_syscall() {
    struct interrupt_context *ctx = intr_context_esp();
    uint32_t intr_num = ctx->eax;
    uint32_t arg1 = ctx->ecx;
    uint32_t arg2 = ctx->edx;
    uint32_t arg3 = ctx->ebx;

    logmsgdf("%s(%d, 0x%x, 0x%x, 0x%x)\n",
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
    logmsgdf("%s(%d) -> %d (0x%x)\n",
            __func__, intr_num, result, result);
    ctx->eax = result;
}
