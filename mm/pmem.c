#include <mm/pmem.h>

#include <mboot.h>
#include <multiboot.h>

#define MEM_DEBUG   0

extern uint _start;
extern uint _end;

#define PAGE_SHIFT  12
#define PAGE_SIZE   (1 << PAGE_SHIFT)

inline uint aligned_back(uint addr) {
    return (addr & ~(PAGE_SIZE - 1));
}

inline uint aligned(uint addr) {
    if (addr & (uint)(PAGE_SIZE - 1))
        return aligned_back(addr) + PAGE_SIZE;
    return addr;
}

/***
  *     Page 'class'
 ***/

struct page {
    
};

void pg_init(struct page *page, bool reserved) {
}

/***
  *     Global pages catalog
 ***/

uint N_PAGES = 0;
uint available_memory = 0;
struct page *mem_map = null;    // will be initialized in pmem_setup()

void pages_set(uint addr, uint size, bool reserved) {
    uint end_page = aligned(addr + size - 1) / PAGE_SIZE;
    if (end_page == 0) return;

    uint i = addr / PAGE_SIZE;
    if ((!reserved) && (addr % PAGE_SIZE)) ++i;

#if MEM_DEBUG
    k_printf("%s pages from %x to %x\n", (reserved? "reserving" : "freeing  "),  i, end_page);
#endif

    for (; i < end_page; ++i) {
        pg_init(mem_map + i, reserved);
    }
    if (reserved) {
        pg_init(mem_map + i, reserved);
    }

    if (N_PAGES <= end_page) N_PAGES = end_page;
    else k_printf("odd thing: N_PAGES(%d) > end_page(%d)\n", N_PAGES, end_page);
}

void pmem_setup(void) {
    mem_map = (struct page *)aligned(&_end);

    struct memory_map *mapping = (struct memory_map *)mboot_mmap_addr();

    int i;
    for (i = 0; i < mboot_mmap_length(); ++i) {
        if (mapping[i].length_low == 0) continue;
        
        /* set mem_map entries */
        switch (mapping[i].type) {
        case 1: /* Free pages */ 
            pages_set(mapping[i].base_addr_low, 
                      mapping[i].length_low, 
                      false);
            available_memory += mapping[i].length_low;
            break;
        case 2: /* Hw-reserved */
            pages_set(mapping[i].base_addr_low,
                      mapping[i].length_low, 
                      true);
            break;
        }
    }

    pg_init(mem_map, true);
}

uint pg_alloc(void) {

}

void pg_free(uint addr) {

}

void pmem_info() {
    struct memory_map *mmmap = (struct memory_map *)mboot_mmap_addr();
    uint i;
    k_printf("\nMemory map [%d]\n", mboot_mmap_length());
    for (i = 0; i < mboot_mmap_length(); ++i) {
        if (mmmap[i].length_low == 0) continue;
        k_printf("%d) sz=%x,bl=%x,bh=%x,ll=%x,lh=%x,t=%x\n", i, mmmap[i].size, 
        mmmap[i].base_addr_low, mmmap[i].base_addr_high,
        mmmap[i].length_low, mmmap[i].length_high, 
        mmmap[i].type);
    }

    k_printf("\nKernel memory: 0x%x-0x%x\nAvailable: %d\n", (uint)&_start, (uint)&_end, available_memory);
    k_printf("Page size: %d\nMem map[%d] at 0x%x\n", PAGE_SIZE, N_PAGES, (uint)mem_map);
}
