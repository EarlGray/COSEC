#include <pmem.h>

#include <arch/mboot.h>
#include <arch/multiboot.h>

#define MEM_DEBUG   (1)

extern uint _start;
extern uint _end;

#define PAGE_SHIFT  12
#define PAGE_SIZE   (1 << PAGE_SHIFT)

inline uint aligned_back(uint addr) {
    return (addr & ~(PAGE_SIZE - 1));
}

/*      aligned(n*PAGE_SIZE) = n*PAGE_SIZE
 *      aligned(n*PAGE_SIZE + 1) = (n+1)*PAGE_SIZE
 */
inline uint aligned(uint addr) {
    if (addr & (uint)(PAGE_SIZE - 1))
        return aligned_back(addr) + PAGE_SIZE;
    return addr;
}


/***
  *     Page 'class'
 ***/

#define PG_RESERVED     1

typedef struct {
    uint used;      // reference count, 0 for a free pageframe
    uint flags;        
} page_frame;

void pg_init(page_frame *pf, bool reserved) {
    if (reserved) 
        pf->flags |= PG_RESERVED;
    
}


/***
  *     Global pages catalog
 ***/

uint n_pages = 0;
uint available_memory = 0;
page_frame *mem_map = null;    // will be initialized in pmem_setup()

void pages_set(uint addr, uint size, bool reserved) {
    uint end_page = aligned(addr + size - 1) / PAGE_SIZE;
    if (end_page == 0) 
        return;

    uint i = addr / PAGE_SIZE;
    if ((!reserved) && (addr % PAGE_SIZE)) ++i;

#if MEM_DEBUG
    k_printf("%s pages from %x to %x; ", (reserved? "reserving" : "freeing  "),  i, end_page - 1);
#endif

    for (; i < end_page; ++i) {
        pg_init(mem_map + i, reserved);
    }
    if (reserved) {
        pg_init(mem_map + i , reserved);
    } else 
        if (n_pages <= end_page) {
            n_pages = end_page;
            k_printf("n_pages = %x", n_pages);
        }
        else k_printf("odd thing: n_pages(%d) > end_page(%d)", n_pages, end_page);
    k_printf("\n");
}

void pmem_setup(void) {
    mem_map = (struct page *)aligned((uint)&_end);

    struct memory_map *mapping = (struct memory_map *)mboot_mmap_addr();

    uint i;
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

    /* reserve realmode interrupt table */
    pg_init(mem_map, true);

    /* reserve kernel and mem_map itself */
    pages_set((uint)&_start, n_pages * sizeof(page_frame), true);
}

uint pg_alloc(void) {
    
    return 0;   
}

void pg_free(uint addr) {

}

void pmem_info() {
    struct memory_map *mmmap = (struct memory_map *)mboot_mmap_addr();
    uint i;
    k_printf("\nMemory map [%d]\n", mboot_mmap_length());
    for (i = 0; i < mboot_mmap_length(); ++i) {
        if (mmmap[i].length_low == 0) continue;
        if (mmmap[i].size > 100) continue;

        k_printf("%d) sz=%x,bl=%x,bh=%x,ll=%x,lh=%x,t=%x\n", i, mmmap[i].size, 
        mmmap[i].base_addr_low, mmmap[i].base_addr_high,
        mmmap[i].length_low, mmmap[i].length_high, 
        mmmap[i].type);
    }

    k_printf("\nKernel memory: 0x%x-0x%x\nAvailable: %d\n", (uint)&_start, (uint)&_end, available_memory);
    k_printf("Page size: %d\nMem map[%d] at 0x%x\n", PAGE_SIZE, n_pages, (uint)mem_map);
}
