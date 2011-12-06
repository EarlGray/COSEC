#include <pmem.h>
#include <log.h>

#include <arch/i386.h>
#include <arch/mboot.h>
#include <arch/multiboot.h>

#include <mm/kheap.h>

#if MEM_DEBUG
#   define mem_logf(msg, ...) logf(msg, __VA_ARGS__)
#else 
#   define mem_logf(msg, ...)    
#endif

/***
  *     Internal declarations
 ***/

/* page_frame.flags */
#define PG_RESERVED     1

typedef struct {
    uint used;      // reference count, 0 for a free pageframe
    uint flags;        
} page_frame;


size_t n_pages = 0;
size_t available_memory = 0;
page_frame *mem_map = null;    // will be initialized in pmem_setup()
index_t last_initialized_page = 0;

extern void _start, _end;


/***
  *     Alignment
 ***/

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

void pg_init(page_frame *pf, bool reserved) {
    if (reserved) 
        pf->flags |= PG_RESERVED;
    last_initialized_page = (index_t)(pf - mem_map);
}


/***
  *     Global pages catalog
 ***/

void pages_set(uint addr, uint size, bool reserved) {
    uint end_page = aligned(addr + size - 1) / PAGE_SIZE;
    if (end_page == 0) 
        return;

    uint i = addr / PAGE_SIZE;
    if ((!reserved) && (addr % PAGE_SIZE)) ++i;

    mem_logf("%s pages from %x to %x; ", (reserved? "reserving" : "freeing  "),  i, end_page - 1);

    for (; i < end_page; ++i) {
        pg_init(mem_map + i, reserved);
    }
    if (reserved) {
        pg_init(mem_map + i , reserved);
    } else 
        if (n_pages <= end_page) {
            n_pages = end_page;
            logf("n_pages = %x", n_pages);
        }
        else logf("odd thing: n_pages(%d) > end_page(%d)", n_pages, end_page);
    log("\n");
}

uint pmem_alloc(size_t pages_count) {
    if (pages_count < 1) return 0;
#warning "Temporary code: no releasing, a more elaborate algorithm required"
    index_t i = last_initialized_page + 1;
    index_t res = i;
    for (; i < n_pages; ++i) {
        if (mem_map[i].used > 0) {
            res = i;
        }
        if ((res - i) >= pages_count) {
            int j;
            mem_logf("Allocating %x pages at [%x]\n", pages_count, res);
            for (j = 0; j < (int)pages_count; ++j) 
                ++ mem_map[res + j].used;
            last_initialized_page = res + j;
            return res;
        }
    }
    return 0;
}

void pmem_setup(void) {
    mem_map = (page_frame *)aligned((uint)&_end);

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

    kheap_setup();
}

uint pg_alloc(void) {
        
    return 0;   
}

void pg_free(uint addr) {

}

void pmem_info() {
    struct memory_map *mmmap = (struct memory_map *)mboot_mmap_addr();
    uint i;
    printf("\nMemory map [%d]\n", mboot_mmap_length());
    for (i = 0; i < mboot_mmap_length(); ++i) {
        if (mmmap[i].length_low == 0) continue;
        if (mmmap[i].size > 100) continue;

        printf("%d) sz=%x,bl=%x,bh=%x,ll=%x,lh=%x,t=%x\n", i, mmmap[i].size, 
        mmmap[i].base_addr_low, mmmap[i].base_addr_high,
        mmmap[i].length_low, mmmap[i].length_high, 
        mmmap[i].type);
    }

    printf("\nKernel memory: 0x%x-0x%x\nAvailable: %d\n", (uint)&_start, (uint)&_end, available_memory);

    uint free_pages = 0, occupied_pages = 0, reserved_pages = 0;
    for (i = 0; i < n_pages; ++i) {
        if (mem_map[i].flags & PG_RESERVED) 
            ++reserved_pages;
        else 
            if (mem_map[i].used > 0) ++occupied_pages;
            else ++free_pages;
    }
    printf("Page size: %d\nMem map[%d] at 0x%x\n", PAGE_SIZE, n_pages, (uint)mem_map);
    printf("Pages count: %x; free: %x, occupied: %x, reserved: %x\n", 
            n_pages, free_pages, occupied_pages, reserved_pages);
    printf("last initialized page is %x\n", last_initialized_page);
}
