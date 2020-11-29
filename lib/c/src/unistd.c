#include <stdbool.h>
#include <stdint.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <cosec/log.h>
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

/*
int stat(const char *pathname, struct stat *statbuf) { return sys_stat(pathname, statbuf); }
int lstat(const char *pathname, struct stat *statbuf) { return sys_lstat(pathname, statbuf); }
*/
int fstat(int fd, struct stat *statbuf) {
    return sys_fstat(fd, statbuf);
}

pid_t fork(void) {
    return sys_fork();
}

int execve(const char *pathname, char *const argv[], char *const envp[]) {
    return sys_execve(pathname, argv, envp);
}

//int execv(const char *pathname, char *const argv[]) { }

int execvp(const char *file, char *const argv[]) {
    logmsgef("%s: TODO\n", __func__);
    theErrNo = ETODO;
    return -1;
}

//int execvpe(const char *file, char *const argv[], char *const argv[]) { }

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

pid_t setsid(void) {
    return sys_setsid();
}

int setpgid(pid_t pid, pid_t pgid) {
    return sys_setpgid(pid, pgid);
}

inline pid_t wait(int *wstatus) {
    return waitpid(-1, wstatus, 0);
}

inline pid_t waitpid(pid_t pid, int *wstatus, int flags) {
    pid_t ret = sys_waitpid(pid, wstatus, flags);
    if (ret < 0) { theErrNo = -ret; ret = -1; }
    return ret;
}

inline int isatty(int fd) {
    struct stat stat;

    if (fstat(fd, &stat)) {
        return 0;  // errno is set
    }

    switch (gnu_dev_major(stat.st_dev)) {
    case 4:     // tty
    case 88:    // pty
        return 1;
    default:
        return 0;
    }
}

int tcsetpgrp(int fd, pid_t pgrp) {
    (void)fd; (void)pgrp;
    logmsgef("%s: TODO", __func__);

    theErrNo = ETODO;
    return -1;
}
