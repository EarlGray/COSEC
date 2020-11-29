#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <sys/errno.h>

#include <bits/alloc_firstfit.h>
#include <bits/libc.h>

#include <cosec/syscall.h>


static void *theHeapEnd = NULL;
static void *theHeap = NULL;


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

/*
 *  Signals
 */
sighandler_t signal(int signum, sighandler_t handler) {
    return sys_signal(signum, handler);
}
