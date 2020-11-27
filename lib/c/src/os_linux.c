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
#include <bits/alloc_firstfit.h>

static void *theHeapEnd = NULL;
static void *theHeap = NULL;

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
intptr_t sys_brk(void *addr) {
    return __syscall1(__NR_brk, (intptr_t)addr);
}
void sys_exit(int status) {
    __syscall1(__NR_exit, status);
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
static inline void * heap_init() {
    const int pagesize = sysconf(_SC_PAGESIZE);
    const size_t initsize = pagesize * 160;    // 640 KB

    /* lazy init */
    if (!theHeapEnd) {
        const size_t pagesize = sysconf(_SC_PAGESIZE);
        uintptr_t p = (uintptr_t)sys_brk((void *)&_end);
        if (p & (pagesize - 1)) {
            // move to the next page
            p = (p & ~(pagesize - 1)) + pagesize;
        }
        theHeapEnd = (void *)p;
    }

    void *heap_start = sbrk(initsize);
    if (!heap_start) panic("Failed to allocate heap");

    return firstfit_new(heap_start, initsize);
}

inline void *kmalloc(size_t size) {
    struct firstfit_allocator *heap = theHeap;
    return firstfit_malloc(heap, size);
}

inline int kfree(void *ptr) {
    struct firstfit_allocator *heap = theHeap;
    firstfit_free(heap, ptr);
    return 0;
}

inline void *krealloc(void *ptr, size_t size) {
    struct firstfit_allocator *heap = theHeap;
    return firstfit_realloc(heap, ptr, size);
}

void *sbrk(intptr_t increment) {

    /* is this just a request about the heap end? */
    if (increment == 0)
        return theHeapEnd;

    void *oldbrk = theHeapEnd;
    void *newbrk = (void *)sys_brk(oldbrk + increment);
    if (newbrk < oldbrk + increment) {
        theErrNo = ENOMEM;
        return (void *)-1;
    }
    theHeapEnd = newbrk;
    return oldbrk;
}


/*
 *  Process
 */
struct auxval {
    /* see sys/auxv.h */
    int key;
    intptr_t value;
};

extern char **environ;
static struct auxval *auxv;

void _init(void *stack) {
    // argc at stack[0]
    int *p = stack + sizeof(int);

    // skip argv
    while (*p) ++p;
    ++p;
    environ = (char **)p;

    // skip environ
    while (*p) ++p;
    ++p;
    auxv = (struct auxval *)p;

    theHeap = heap_init();
}

void _exit(int status) {
    sys_exit(status);
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
