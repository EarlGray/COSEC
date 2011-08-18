#include <pmem.h>
#include <mm/kheap.h>
#include <mm/firstfit.h>

#include <arch/i386.h>

#define KHEAP_INITIAL_SIZE  0x100000

struct firstfit_allocator *theHeap;

void kheap_setup(void) {
    void *start_heap_addr = pmem_alloc(KHEAP_INITIAL_SIZE / PAGE_SIZE + 1) * PAGE_SIZE;
    theHeap = firstfit_new(start_heap_addr, KHEAP_INITIAL_SIZE);
#if MEM_DEBUG
    k_printf("theHeap at *%x\n", (ptr_t)theHeap);
#endif 
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
