#include <stdbool.h>
#include <stdint.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <sys/errno.h>

#include <cosec/sys.h>

#include <bits/libc.h>

inline long sysconf(int name) {
    switch (name) {
    case _SC_PAGESIZE:
        return 0x1000; // TODO: unify with include/conf.h
    default:
        theErrNo = EINVAL;
        return -1;
    }
}

ssize_t write(int fd, const void *buf, size_t count) {
    return sys_write(fd, buf, count);
}

void abort(void) {
    // TODO: unlock SIGABRT if blocked
    raise(SIGABRT);
}

int raise(int sig) {
    kill(getpid(), sig);
}

int kill(pid_t pid, int sig) {
    return sys_kill(pid, sig);
}

pid_t getpid(void) {
    // TODO: cache it?
    return sys_getpid();
}
