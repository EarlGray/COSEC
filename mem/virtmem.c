#include <mem/virtmem.h>

#include <mem/paging.h>
#include <mem/virtmem.h>

// TODO: need an AVL tree here to speed access

struct vm_area {
    size_t len;         // pages count
    size_t n_pf;        // used pageframes count
    count_t used;       // how many processes use it

    uint32_t vm_flags;
};

void vmem_setup(void) {
    
}

void memory_setup(void) {
#if PAGING
    paging_setup();
#endif
    pmem_setup();
    vmem_setup();
}
