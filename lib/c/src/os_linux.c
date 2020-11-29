#if LINUX

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <signal.h>
#include <sys/errno.h>
#include <time.h>
#include <unistd.h>

#define SYSCALL_NO_TLS  1
#include <linux/unistd_i386.h>
#include <linux/syscall_i386.h>
#include <linux/times.h>

#include <bits/libc.h>

void exit(int status) {
    __syscall1(SYS_exit, status);
}

int sys_chdir(const char *path) {
    return __syscall1(SYS_chdir, (intptr_t)path);
}
int sys_fstat(int fd, void *stat) {
    return __syscall2(SYS_fstat, fd, (intptr_t)stat);
}
int sys_close(int fd) {
    return __syscall1(SYS_close, fd);
}
pid_t sys_getpid(void) {
    return __syscall0(SYS_getpid);
}
int sys_setpgid(pid_t pid, pid_t pgid) {
    return __syscall2(SYS_setpgid, pid, pgid);
}
pid_t sys_setsid(void) {
    return __syscall0(SYS_setsid);
}
int sys_ftruncate(int fd, off_t length) {
    return __syscall2(SYS_ftruncate, fd, length);
}
int sys_link(const char *oldpath, const char *newpath) {
    return __syscall2(SYS_link, (intptr_t)oldpath, (intptr_t)newpath);
}
off_t sys_lseek(int fd, off_t offset, int whence) {
    return __syscall3(SYS_lseek, fd, offset, whence);
}
int sys_mkdir(const char *pathname, mode_t mode) {
    return __syscall2(SYS_mkdir, (intptr_t)pathname, mode);
}
int sys_open(const char *path, int flags) {
    return __syscall2(SYS_open, (int32_t)path, flags);
}
int sys_read(int fd, void *buf, size_t count) {
    return __syscall3(SYS_read, fd, (intptr_t)buf, count);
}
int sys_rename(const char *oldpath, const char *newpath) {
    return __syscall2(SYS_rename, (intptr_t)oldpath, (intptr_t)newpath);
}
int sys_rmdir(const char *pathname) {
    return __syscall1(SYS_rmdir, (intptr_t)pathname);
}
int sys_write(int fd, const void *buf, size_t count) {
    return __syscall3(SYS_write, fd, (intptr_t)buf, count);
}
int sys_unlink(const char *pathname) {
    return __syscall1(SYS_unlink, (intptr_t)pathname);
}
clock_t sys_times(struct tms *tms) {
    return __syscall1(SYS_times, (intptr_t)tms);
}
intptr_t sys_brk(void *addr) {
    return __syscall1(SYS_brk, (intptr_t)addr);
}
intptr_t sys_signal(int signum, sighandler_t handler) {
    struct sigaction sigact = {
        .sa_handler = handler,
        .sa_sigaction = NULL,
        .sa_mask = 0,
        .sa_flags = 0,
    };
    return __syscall3(SYS_signal, signum, (intptr_t)&sigact, 0);
}
pid_t sys_fork(void) {
    return __syscall0(SYS_fork);
}
int sys_execve(const char *pathname, char *const argv[], char *const envp[]) {
    return __syscall3(SYS_execve, (intptr_t)pathname, (intptr_t)argv, (intptr_t)envp);
}
int sys_kill(pid_t pid, int sig) {
    return __syscall2(SYS_kill, pid, sig);
}
pid_t sys_waitpid(pid_t pid, int *wstatus, int flags) {
    return __syscall3(SYS_waitpid, pid, (intptr_t)wstatus, flags);
    //return __syscall4(SYS_wait4, pid, (intptr_t)wstatus, flags, 0);
}
void sys_exit(int status) {
    __syscall1(SYS_exit, status);
}

/*
 *  Time
 */
inline time_t time(time_t *tloc) {
    int32_t epoch = __syscall0(SYS_time);
    if (tloc) *tloc = (time_t)epoch;
    return (time_t)epoch;
}

clock_t clock() {
    struct tms tms;
    return sys_times(&tms);
}


void panic(const char *msg) {
    fprintf(stderr, "FATAL: %s\n", msg);
    abort();
}

int vlprintf(const char *fmt, va_list ap) {
    (void)fmt;
    (void)ap;
    /* TODO */
    return 0;
}

#endif  // LINUX
