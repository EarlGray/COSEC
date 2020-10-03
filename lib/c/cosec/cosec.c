#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>

#include <cosec/fs.h>
#include <cosec/syscall.h>

/*
 *  Syscalls
 */
inline void exit(int status) {
    syscall(SYS_EXIT, status, 0, 0);
}
inline int sys_mkdir(const char *pathname, mode_t mode) {
    return syscall(SYS_MKDIR, (uintptr_t)pathname, mode, 0);
}
inline int sys_rmdir(const char *pathname) {
    return syscall(SYS_RMDIR, (uintptr_t)pathname, 0, 0);
}
inline int sys_chdir(const char *path) {
    return syscall(SYS_CHDIR, (uintptr_t)path, 0, 0);
}
inline int sys_link(const char *oldpath, const char *newpath) {
    return syscall(SYS_LINK, (uintptr_t)oldpath, (uintptr_t)newpath, 0);
}
inline int sys_unlink(const char *pathname) {
    return syscall(SYS_UNLINK, (uintptr_t)pathname, 0, 0);
}
inline int sys_rename(const char *oldpath, const char *newpath) {
    return syscall(SYS_RENAME, (uintptr_t)oldpath, (uintptr_t)newpath, 0);
}

// inline int sys_symlink(const char *oldpath, const char *newpath) { return syscall(SYS_
// inline char *sys_pwd(char *buf) { return (char *)syscall() };
// inline int sys_lsdir(const char *pathname, struct cosec_dirent *dirs, count_t count);


inline int sys_open(const char *pathname, int flags) {
    return syscall(SYS_OPEN, (uintptr_t)pathname, flags, 0);
}
inline int sys_read(int fd, void *buf, size_t count) {
    return syscall(SYS_READ, fd, (uintptr_t)buf, count);
}
inline int sys_write(int fd, const void *buf, size_t count) {
    return syscall(SYS_WRITE, fd, (uintptr_t)buf, count);
}
inline int sys_close(int fd) {
    return syscall(SYS_CLOSE, fd, 0, 0);
}

inline off_t sys_lseek(int fd, off_t offset, int whence) {
    return syscall(SYS_LSEEK, fd, offset, whence);
}
inline int sys_ftruncate(int fd, off_t length) {
    return syscall(SYS_TRUNC, fd, length, 0);
}

/*
 *  Process
 */
void panic(const char *msg) {
    fprintf(stderr, "PANIC: %s\n", msg);
    abort();
}

void abort(void) {
    // TODO: raise SIGABRT
}

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
