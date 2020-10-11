#include "cosec/syscall.h"
#include <unistd.h>
#include <signal.h>

#include <cosec/sys.h>

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
