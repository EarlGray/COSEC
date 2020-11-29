#if COSEC

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>

#include <signal.h>
#include <cosec/log.h>
#include <cosec/sys.h>

/*
 *  Syscalls
 */
inline int sys_getpid(void) {
	return __syscall0(SYS_getpid);
}
inline int sys_setpgid(pid_t pid, pid_t pgid) {
    return __syscall2(SYS_setpgid, pid, pgid);
}
inline pid_t sys_setsid(void) {
    return __syscall0(SYS_setsid);
}
inline int sys_fstat(int fd, void *stat) {
    return __syscall2(SYS_fstat, fd, (intptr_t)stat);
}
inline int sys_mkdir(const char *pathname, mode_t mode) {
    return __syscall2(SYS_mkdir, (intptr_t)pathname, mode);
}
inline int sys_rmdir(const char *pathname) {
    return __syscall1(SYS_rmdir, (intptr_t)pathname);
}
inline int sys_chdir(const char *path) {
    return __syscall1(SYS_chdir, (intptr_t)path);
}
inline int sys_link(const char *oldpath, const char *newpath) {
    return __syscall2(SYS_link, (intptr_t)oldpath, (intptr_t)newpath);
}
inline int sys_unlink(const char *pathname) {
    return __syscall1(SYS_unlink, (intptr_t)pathname);
}
inline int sys_rename(const char *oldpath, const char *newpath) {
    return __syscall2(SYS_rename, (intptr_t)oldpath, (intptr_t)newpath);
}

inline int sys_open(const char *pathname, int flags) {
    return __syscall2(SYS_open, (intptr_t)pathname, flags);
}
inline int sys_read(int fd, void *buf, size_t count) {
    return __syscall3(SYS_read, fd, (intptr_t)buf, count);
}
inline int sys_write(int fd, const void *buf, size_t count) {
    return __syscall3(SYS_write, fd, (intptr_t)buf, count);
}
inline int sys_close(int fd) {
    return __syscall1(SYS_close, fd);
}

inline pid_t sys_fork(void) {
    return __syscall0(SYS_fork);
}
inline int sys_execve(const char *pathname, char *const argv[], char *const envp[]) {
    return __syscall3(SYS_execve, (intptr_t)pathname, (intptr_t)argv, (intptr_t)envp);
}
inline int sys_kill(pid_t pid, int sig) {
    return __syscall2(SYS_kill, pid, sig);
}
inline pid_t sys_waitpid(pid_t pid, int *wstatus, int flags) {
    return __syscall3(SYS_waitpid, pid, (intptr_t)wstatus, flags);
}
inline sighandler_t sys_signal(int signum, sighandler_t handler) {
    struct sigaction sigact = {
        .sa_handler = handler,
        .sa_sigaction = NULL,
        .sa_mask = 0,
        .sa_flags = 0,
    };
    return (sighandler_t)__syscall3(
            SYS_sigaction, signum, (intptr_t)&sigact, 0
    );
}
inline off_t sys_lseek(int fd, off_t offset, int whence) {
    return __syscall3(SYS_lseek, fd, offset, whence);
}
inline int sys_ftruncate(int fd, off_t length) {
    return __syscall2(SYS_trunc, fd, length);
}

inline time_t sys_time(time_t *tloc) {
    int32_t epoch = __syscall0(SYS_time);
    if (tloc) *tloc = (time_t)epoch;
    return (time_t)epoch;
}

inline intptr_t sys_brk(void *addr) {
    return (intptr_t)__syscall1(SYS_brk, (intptr_t)addr);
}
inline void sys_exit(int status) {
    __syscall1(SYS_exit, status);
}

// inline int sys_symlink(const char *oldpath, const char *newpath) { return syscall(SYS_
// inline char *sys_pwd(char *buf) { return (char *)syscall() };
// inline int sys_lsdir(const char *pathname, struct cosec_dirent *dirs, count_t count);

void exit(int status) {
    __syscall1(SYS_exit, status);
}

void panic(const char *msg) {
    fprintf(stderr, "FATAL: %s\n", msg);
    abort();
}


/*
 *  Time
 */
clock_t clock(void) {
    fprintf(stderr, "%s: TODO\n", __func__);
    return -1;
}

inline time_t time(time_t *tloc) {
    return sys_time(tloc);
}

int vlprintf(const char *fmt, va_list ap) {
    /* TODO: do snprintf to a buffer, pass the buffer */
    return __syscall1(SYS_print, (intptr_t)&fmt);
}

#endif // COSEC
