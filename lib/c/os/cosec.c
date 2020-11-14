#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>

#include <cosec/sys.h>

/*
 *  Syscalls
 */
inline void exit(int status) {
    __syscall1(SYS_EXIT, status);
}

inline int sys_getpid(void) {
	return __syscall0(SYS_GETPID);
}
inline int sys_mkdir(const char *pathname, mode_t mode) {
    return __syscall2(SYS_MKDIR, (intptr_t)pathname, mode);
}
inline int sys_rmdir(const char *pathname) {
    return __syscall1(SYS_RMDIR, (intptr_t)pathname);
}
inline int sys_chdir(const char *path) {
    return __syscall1(SYS_CHDIR, (intptr_t)path);
}
inline int sys_link(const char *oldpath, const char *newpath) {
    return __syscall2(SYS_LINK, (intptr_t)oldpath, (intptr_t)newpath);
}
inline int sys_unlink(const char *pathname) {
    return __syscall1(SYS_UNLINK, (intptr_t)pathname);
}
inline int sys_rename(const char *oldpath, const char *newpath) {
    return __syscall2(SYS_RENAME, (intptr_t)oldpath, (intptr_t)newpath);
}

inline int sys_open(const char *pathname, int flags) {
    return __syscall2(SYS_OPEN, (intptr_t)pathname, flags);
}
inline int sys_read(int fd, void *buf, size_t count) {
    return __syscall3(SYS_READ, fd, (intptr_t)buf, count);
}
inline int sys_write(int fd, const void *buf, size_t count) {
    return __syscall3(SYS_WRITE, fd, (intptr_t)buf, count);
}
inline int sys_close(int fd) {
    return __syscall1(SYS_CLOSE, fd);
}

inline int sys_kill(pid_t pid, int sig) {
    return __syscall2(SYS_KILL, pid, sig);
}
inline off_t sys_lseek(int fd, off_t offset, int whence) {
    return __syscall3(SYS_LSEEK, fd, offset, whence);
}
inline int sys_ftruncate(int fd, off_t length) {
    return __syscall2(SYS_TRUNC, fd, length);
}

inline time_t sys_time(time_t *tloc) {
    int32_t epoch = __syscall0(SYS_TIME);
    if (tloc) *tloc = (time_t)epoch;
    return (time_t)epoch;
}

// inline int sys_symlink(const char *oldpath, const char *newpath) { return syscall(SYS_
// inline char *sys_pwd(char *buf) { return (char *)syscall() };
// inline int sys_lsdir(const char *pathname, struct cosec_dirent *dirs, count_t count);


/*
 *  Memory management
 */

void *kmalloc(size_t size) {
    fprintf(stderr, "%s: TODO\n", __func__);
    return NULL;
}

int kfree(void *ptr) {
    fprintf(stderr, "%s: TODO\n", __func__);
    return 0;
}

void *krealloc(void *ptr, size_t size) {
    fprintf(stderr, "%s: TODO\n", __func__);
    return NULL;
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
