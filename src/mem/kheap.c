#include <cosec/log.h>

#include <mem/pmem.h>
#include <mem/kheap.h>
#include <mem/ff_alloc.h>

#include <arch/i386.h>

#define KHEAP_INITIAL_SIZE  (256 * PAGE_SIZE)

#if (1)
#   define mem_logf(msg, ...) logmsgf(msg, __VA_ARGS__)
#   define mem_corruption_check() do { \
       void *corrupted = firstfit_corruption(theHeap); \
       if (corrupted) logmsgef("kheap corrupted at *0x%x\n", corrupted); \
    } while (0);
#else 
#   define mem_logf(msg, ...)
#   define mem_corruption_check()
#endif

struct firstfit_allocator *theHeap;

void kheap_setup(void) {
    void *start_heap_addr = pmem_alloc(KHEAP_INITIAL_SIZE / PAGE_SIZE + 1);
    if (0 == start_heap_addr) {
        k_printf("theHeap allocation failed\n");
        return;
    }

    theHeap = firstfit_new(start_heap_addr, KHEAP_INITIAL_SIZE);
    k_printf("theHeap at *%x (until *%x)\n",
             (ptr_t)theHeap, (ptr_t)theHeap + KHEAP_INITIAL_SIZE);
}

void *kmalloc(size_t size) {
    void * ptr = firstfit_malloc(theHeap, size);
    mem_logf("~~ kmalloc(0x%x) -> *0x%x\n", size, ptr);
    mem_corruption_check();
    return ptr;
}

int kfree(void *p) {
    mem_logf("~~ kfree(*0x%x)\n", p);
    firstfit_free(theHeap, p);
    mem_corruption_check();
    return 0;
}

void *krealloc(void *p, size_t size) {
    void *r = firstfit_realloc(theHeap, p, size);
    mem_logf("~~ krealloc(*0x%x, 0x%x) -> *0x%x\n", p, size, r);
    mem_corruption_check();
    return r;
}

void kheap_info(void) {
    heap_info(theHeap);
}

void * kheap_check(void) {
    return firstfit_corruption(theHeap);
}
