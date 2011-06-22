#include <mm/physmem.h>

#include <mboot.h>
#include <multiboot.h>

extern uint _start;
extern uint _end;

#define PAGE_SHIFT  12
#define PAGE_SIZE   4096

inline aligned_back(uint addr) {
    return (addr & ~(PAGE_SIZE - 1));
}

inline aligned(uint addr) {
    if (0 == (addr & (PAGE_SIZE - 1)))
        return addr;
    return aligned_back(addr) + PAGE_SIZE;
}

/***
  *     Page 'class'
 ***/

struct page {
    
};

void page_init(struct page *page, bool reserved) {
}

/***
  *     Global pages catalog
 ***/

struct page *mem_map = null;    // will be initialized in phmem_setup()

void pages_set(uint addr, uint size, bool used) {
    addr &= ~(PAGE_SIZE - 1);
    
}


void phmem_setup(void) {
    mem_map = (struct page *)&_end;

    struct memory_map *mapping = (struct memory_map *)mboot_mmap_addr();
    uint i;
    for (i = 0; i < mboot_mmap_length(); ++i) {
        if (mapping[i].length_low == 0) continue;
        
        /* set mem_map entries */
        switch (mapping[i].type) {
        case 1: /* Free pages */ 
            break;
        case 2: /* Hw-reserved */
            break;
        }
    }

#ifdef VERBOSE
    k_printf("kernel memory : 0x%x - 0x%x\n", (uint)&_start, (uint)&_end);
#endif
}
