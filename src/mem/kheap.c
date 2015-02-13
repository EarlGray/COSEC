#include <log.h>
#include <mem/pmem.h>
#include <mem/kheap.h>
#include <mem/firstfit.h>

#include <arch/i386.h>

#define KHEAP_INITIAL_SIZE  (256 * PAGE_SIZE)

#if MEM_DEBUG & 0
#   define mem_logf(msg, ...) logf(msg, __VA_ARGS__)
#else 
#   define mem_logf(msg, ...)
#endif

struct firstfit_allocator *theHeap;

void kheap_setup(void) {
    void *start_heap_addr = pmem_alloc(KHEAP_INITIAL_SIZE / PAGE_SIZE + 1);
    if (0 == start_heap_addr) {
        k_printf("theHeap allocation failed\n");
        return;
    }

    theHeap = firstfit_new(start_heap_addr, KHEAP_INITIAL_SIZE);
    mem_logf("theHeap at *%x\n", (ptr_t)theHeap);
}

void *kmalloc(size_t size) {
    void * ptr = firstfit_malloc(theHeap, size);
    mem_logf("kmalloc(0x%x) -> *0x%x\n", size, ptr);
    return ptr;
}

int kfree(void *p) {
    mem_logf("kfree(*0x%x)\n", p);
    firstfit_free(theHeap, p);
    return 0;
}

void *krealloc(void *p, size_t size) {
    return firstfit_realloc(theHeap, p, size);
}

void kheap_info(void) {
    heap_info(theHeap);
}

void * kheap_check(void) {
    return firstfit_corruption(theHeap);
}
