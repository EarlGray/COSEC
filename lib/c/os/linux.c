#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>


#define SYSCALL_NO_TLS  1
#include <linux/unistd_i386.h>
#include <linux/syscall_i386.h>
#include <linux/times.h>

void exit(int status) {
    __syscall1(__NR_exit, status);
}

int sys_chdir(const char *path) {
    return __syscall1(__NR_chdir, (intptr_t)path);
}
int sys_close(int fd) {
    return __syscall1(__NR_close, fd);
}
pid_t sys_getpid(void) {
    return __syscall0(__NR_kill);
}
int sys_ftruncate(int fd, off_t length) {
    return __syscall2(__NR_ftruncate, fd, length);
}
int sys_link(const char *oldpath, const char *newpath) {
    return __syscall2(__NR_link, (intptr_t)oldpath, (intptr_t)newpath);
}
off_t sys_lseek(int fd, off_t offset, int whence) {
    return __syscall3(__NR_lseek, fd, offset, whence);
}
int sys_mkdir(const char *pathname, mode_t mode) {
    return __syscall2(__NR_mkdir, (intptr_t)pathname, mode);
}
int sys_open(const char *path, int flags) {
    return __syscall2(__NR_open, (int32_t)path, flags);
}
int sys_read(int fd, void *buf, size_t count) {
    return __syscall3(__NR_read, fd, (intptr_t)buf, count);
}
int sys_rename(const char *oldpath, const char *newpath) {
    return __syscall2(__NR_rename, (intptr_t)oldpath, (intptr_t)newpath);
}
int sys_rmdir(const char *pathname) {
    return __syscall1(__NR_rmdir, (intptr_t)pathname);
}
int sys_write(int fd, const void *buf, size_t count) {
    return __syscall3(__NR_write, fd, (intptr_t)buf, count);
}
int sys_unlink(const char *pathname) {
    return __syscall1(__NR_unlink, (intptr_t)pathname);
}
int sys_kill(pid_t pid, int sig) {
    return __syscall2(__NR_kill, pid, sig);
}
clock_t sys_times(struct tms *tms) {
    return __syscall1(__NR_times, (intptr_t)tms);
}

/*
 *  Time
 */
inline time_t time(time_t *tloc) {
    int32_t epoch = __syscall0(__NR_time);
    if (tloc) *tloc = (time_t)epoch;
    return (time_t)epoch;
}

clock_t clock() {
    struct tms tms;
    return sys_times(&tms);
}

/*
 *  Memory
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
    (void)size;
    fprintf(stderr, "%s: TODO\n", __func__);
    return NULL;
}

void panic(const char *msg) {
    fprintf(stderr, "FATAL: %s\n", msg);
    abort();
}
