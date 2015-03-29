#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <process.h>
#include <dev/tty.h>
#include <fs/vfs.h>

#include <log.h>

/*
 *  Global state
 */
pid_t theCurrPID;
pid_t theAllocPID = 1;

process theInitProc;
process * theProcTable[NPROC_MAX] = { 0 };


/*
 *  PID management
 */

static pid_t
alloc_pid(void) {
    pid_t i;
    for (i = theAllocPID; i < NPROC_MAX; ++i)
        if (NULL == theProcTable[i]) {
            theAllocPID = i;
            return i;
        }

    for (i = 1; i < theAllocPID; ++i)
        if (NULL == theProcTable[i]) {
            theAllocPID = i;
            return i;
        }

    return 0;
}

process * proc_by_pid(pid_t pid) {
    if (pid > NPROC_MAX) return 0;
    return theProcTable[pid];
}

pid_t current_pid(void) {
    return theCurrPID;
}

process * current_proc(void) {
    return theProcTable[theCurrPID];
}

int alloc_fd_for_pid(pid_t pid) {
    const char *funcname = __FUNCTION__;
    int i;
    process *p = proc_by_pid(pid);
    return_dbg_if(!p, EKERN, "%s: no process with pid %d\n", funcname, pid);

    for (i = 0; i < N_PROCESS_FDS; ++i)
        if (p->ps_fds[i].fd_ino < 1)
            return i;

    return -1;
}

filedescr * get_filedescr_for_pid(pid_t pid, int fd) {
    const char *funcname = __FUNCTION__;
    process *p = proc_by_pid(pid);
    return_dbg_if(!p, EKERN,
            "%s: no process with pid %d\n", funcname, pid);
    return_dbg_if(!((0 <= fd) && (fd < N_PROCESS_FDS)), NULL,
            "%s: fd=%d out of range\n", funcname, fd);

    return p->ps_fds + fd;
}

extern char kern_stack;

void proc_setup(void) {
    const char *funcname = __FUNCTION__;
    /* invalid */
    theProcTable[0] = NULL;

    /* there is the init process at start up */
    theCurrPID = 1;
    theProcTable[theCurrPID] = &theInitProc;

    theInitProc.ps_pid = theCurrPID;
    theInitProc.ps_ppid = 0;
    theInitProc.ps_tty = CONSOLE_TTY;
    theInitProc.ps_kernstack = &kern_stack;

    /* temporary hack */
    /* init process should initialize its descriptors from userspace */
    int ret = 0;
    mountnode *sb = NULL;
    inode_t ino = 0;

    ret = vfs_lookup("/dev/tty0", &sb, &ino);
    returnv_err_if(ret, "%s: vfs_lookup('/dev/tty0'): %s", funcname, strerror(ret));

    filedescr *infd = theInitProc.ps_fds + STDIN_FILENO;
    filedescr *outfd = theInitProc.ps_fds + STDOUT_FILENO;
    filedescr *errfd = theInitProc.ps_fds + STDERR_FILENO;

    infd->fd_sb  = outfd->fd_sb  = errfd->fd_sb  = sb;
    infd->fd_ino = outfd->fd_ino = errfd->fd_ino = ino;
    infd->fd_pos = outfd->fd_pos = errfd->fd_pos = -1;
}

