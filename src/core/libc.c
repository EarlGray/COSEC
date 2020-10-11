#include <stdlib.h>
#include <stdbool.h>
#include <setjmp.h>

#include <cosec/log.h>
#include <machine/setjmp.h>

#include "arch/i386.h"
#include "libc.h"

struct {
    jmp_buf exit_env;
    bool actual_exit;
    int status;
} theExitInfo = {
    .exit_env = { 0 },
    .actual_exit = false,
    .status = 0,
};

int exitpoint(void) {
    int ret = setjmp(theExitInfo.exit_env);

    if (ret) {
        // actual exit
        theExitInfo.status = ret;
        theExitInfo.actual_exit = true;
        theExitInfo.exit_env[0] = 0;
        if (ret == EXITENV_ABORTED)
            logmsg("...aborted\n");
        return (ret == EXITENV_SUCCESS ? EXIT_SUCCESS : ret);
    } else {
        // exit point set
        theExitInfo.status = EXITENV_SUCCESS;
        theExitInfo.actual_exit = false;
        return EXITENV_EXITPOINT;
    }
}

void exit(int status) {
    if (theExitInfo.exit_env[0] == 0) {
        logmsge("exit: exitpoint not set");
        return;
    }
    /* TODO : atexit handlers */
    longjmp(theExitInfo.exit_env,
            (status == EXIT_SUCCESS ? EXITENV_SUCCESS : status));
}

void abort(void) {
    longjmp(theExitInfo.exit_env, EXITENV_ABORTED);
}

int setjmp(jmp_buf env) {
    return i386_setjmp(env);
}

void longjmp(jmp_buf env, int val) {
    i386_longjmp(env, val);
}
