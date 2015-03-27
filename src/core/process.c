#include <stdlib.h>
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

extern char kern_stack;

void proc_setup(void) {
    /* invalid */
    theProcTable[0] = NULL;

    /* there is the init process at start up */
    theCurrPID = 1;
    theProcTable[theCurrPID] = &theInitProc;

    theInitProc.ps_pid = theCurrPID;
    theInitProc.ps_ppid = 0;
    theInitProc.ps_tty = CONSOLE_TTY;
    theInitProc.ps_kernstack = &kern_stack;
}

