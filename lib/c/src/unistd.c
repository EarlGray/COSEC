#include <stdbool.h>
#include <stdint.h>
#include <signal.h>
#include <time.h>
#include <termios.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/syscall.h>

#include <cosec/log.h>

#include <bits/libc.h>

extern char **environ;

static inline int negative_to_errno(int result) {
    if (result < 0) { theErrNo = -result; result = -1; }
    return result;
}

inline long sysconf(int name) {
    switch (name) {
    case _SC_PAGESIZE:
        return 0x1000; // TODO: unify with include/conf.h
    default:
        theErrNo = EINVAL;
        return -1;
    }
}

int sys_ioctl(int fd, unsigned long request, void *argp) {
    return __syscall3(SYS_ioctl, fd, request, (intptr_t)argp);
}
//inline size_t confstr(int name, char *buf, size_t len) { }

ssize_t write(int fd, const void *buf, size_t count) {
    return sys_write(fd, buf, count);
}

/*
int stat(const char *pathname, struct stat *statbuf) { return sys_stat(pathname, statbuf); }
int lstat(const char *pathname, struct stat *statbuf) { return sys_lstat(pathname, statbuf); }
*/
int fstat(int fd, struct stat *statbuf) {
    return negative_to_errno(
        sys_fstat(fd, statbuf)
    );
}

pid_t fork(void) {
    return negative_to_errno(
        sys_fork()
    );
}

int execve(const char *pathname, char *const argv[], char *const envp[]) {
    if (pathname[0] != '/') {
        logmsgef("%s('%s'): need an absolute path\n", __func__, pathname);
        theErrNo = ENOENT;
        return -1;
    }

    return negative_to_errno(
        sys_execve(pathname, argv, envp)
    );
}

//int execv(const char *pathname, char *const argv[]) { }

int execvp(const char *file, char *const argv[]) {
    /* TODO: find the directory for file if needed */

    return execve(file, argv, environ);
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
    return negative_to_errno(
        sys_kill(pid, sig)
    );
}

pid_t getpid(void) {
    // TODO: cache it? but forks.
    return negative_to_errno(sys_getpid());
}

pid_t setsid(void) {
    return negative_to_errno(sys_setsid());
}

int setpgid(pid_t pid, pid_t pgid) {
    return negative_to_errno(sys_setpgid(pid, pgid));
}

inline pid_t wait(int *wstatus) {
    return waitpid(-1, wstatus, 0);
}

inline pid_t waitpid(pid_t pid, int *wstatus, int flags) {
    return negative_to_errno(
        sys_waitpid(pid, wstatus, flags)
    );
}

inline int isatty(int fd) {
    struct stat stat;

    if (fstat(fd, &stat)) {
        return 0;  // errno is set
    }

    switch (gnu_dev_major(stat.st_rdev)) {
    case 4:     // tty
    case 88:    // pty
        return 1;
    default:
        return 0;
    }
}

int tcsetpgrp(int fd, pid_t pgrp) {
    pid_t *pid = &pgrp;
    return negative_to_errno(
        sys_ioctl(fd, TIOCSPGRP, pid)
    );
}
