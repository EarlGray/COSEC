#include <pmem.h>
#include <log.h>
#include <mm/kheap.h>
#include <mm/firstfit.h>

#include <arch/i386.h>

#define KHEAP_INITIAL_SIZE  0x100000

#if MEM_DEBUG
#   define mem_logf(msg, ...) logf(msg, __VA_ARGS__)
#else 
#   define mem_logf(msg, ...)
#endif

struct firstfit_allocator *theHeap;

void kheap_setup(void) {
    void *start_heap_addr = (void *)( pmem_alloc(KHEAP_INITIAL_SIZE / PAGE_SIZE + 1) * PAGE_SIZE );
    theHeap = firstfit_new(start_heap_addr, KHEAP_INITIAL_SIZE);
    mem_logf("theHeap at *%x\n", (ptr_t)theHeap);
}

void *kmalloc(size_t size) {
    return firstfit_malloc(theHeap, size);
}

int kfree(void *p) {
    firstfit_free(theHeap, p);
    return 0;
}

void kheap_info(void) {
    heap_info(theHeap);
}
