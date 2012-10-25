#include <pmem.h>
#include <log.h>

/*
 *   This file represents physical memory "object"
 * and its methods.
 *   Physical memory is treated as a heap of pageframes.
 *   This is reflected in thePageframeMap, which resides right after
 * the kernel code: it is array of page_frame structures with
 * length n_pages;
 */

#include <arch/i386.h>
#include <arch/mboot.h>

#include <mm/kheap.h>
#include <mm/paging.h>

#if MEM_DEBUG
#   define mem_logf(msg, ...) logf(msg, __VA_ARGS__)
#else
#   define mem_logf(msg, ...)
#endif

extern void _start, _end;

/***
  *     Alignment
 ***/

/*      aligned(n*PAGE_SIZE) = n*PAGE_SIZE
 *      aligned(n*PAGE_SIZE + 1) = (n+1)*PAGE_SIZE
 */
static inline uint aligned(uint addr) {
    if (addr % PAGE_SIZE)
        return PAGE_SIZE * (1 + addr / PAGE_SIZE);
    return addr;
}

static inline uint aligned_back(uint addr) {
    return addr / PAGE_SIZE;
}

/***
  *     Internal declarations
 ***/


/* page_frame.flags */
// lowest two bits: pageframe type
#define PF_FREE         0
#define PF_USED         1
#define PF_CACHE        2
#define PF_RESERVED     3
// flags:3 : is this cache used right now
#define CACHE_IN_USE    0x4

typedef struct pageframe {
   uint flags;                      //
   struct pageframe *next, *prev;   // in the pageframe group (reserved/free/cache/used)
   vm_area_t *vma;                  // virtual memory manager
   index_t vm_offset;               // index in vma
} pageframe_t;

#if (1)
// the global pageframe table:
pageframe_t *the_pageframe_map = null;
size_t      pageframe_map_len = 0;

// list of free pageframes:
pageframe_t *free_pageframes = null;
size_t      n_free_pageframes = 0;

// list of pageframes asssigned to virtual memory:
pageframe_t *used_pageframes = null;
size_t      n_used_pageframes = 0;

pageframe_t *cache_pageframes = null;
size_t      n_cache_pageframes = 0;

pageframe_t *reserved_pageframes = null;
size_t      n_reserved_pageframes = 0;

void pmem_setup(void) {
    int i;
    struct memory_map *mapping = (struct emory_map *)mboot_mmap_addr();
    ptr_t pfmap = (ptr_t) aligned((ptr_t)&_end);

    paging_setup();

    // switch page directory to in-kernel one

        
    /// determine pageframe_map_len
    for (i = 0; i < mboot_mmap_length(); ++i) {
        struct memory_map *m = mapping + i;
        if (m->type == 1) 
            pageframe_map_len = (m->base_addr_low + m->length_low) / PAGE_SIZE;
    }

    /// allocate the pageframe map
    // skip all multiboot modules
    module_t *mods = null;
    size_t n_mods = 0;
    mboot_modules_info(&n_mods, &mods);
    for (i = 0; i < n_mods; ++i) {
        if (pfmap < mods[i].mod_end) pfmap = mods[i].mod_end;
    }

    ptr_t pfmap_end = pfmap + sizeof(pageframe_t) * pageframe_map_len;
    mem_logf("pfmap at *%x:*%x (0x%x pageframes)\n", pfmap, pfmap_end, pageframe_map_len);

    the_pageframe_map = (pageframe_t *) aligned((ptr_t)pfmap);

    // make all reserved
    reserved_pageframes = the_pageframe_map + 0;
}

void mark_reserved(pageframe_t *pf) {
    
}

index_t pmem_alloc(size_t pages_count) {
    return 0;
}

void pmem_info(void) {
}

#else

#define PG_RESERVED 1
#define PG_RESERVED_USE 0xffffffff

typedef struct page_frame {
    uint flags;
    index_t prev, next;   // for free pages: next available
    uint used;
} page_frame_t;

size_t available_memory = 0;

size_t n_pages = 0;
size_t n_available_pages = 0;

page_frame_t *thePageframeMap = null;    // will be initialized in pmem_setup()
index_t current_page = 0;




/***
  *     Page 'class'
 ***/

#define pg_is_used(index)       (thePageframeMap[index].used)
#define pg_is_reserved(index)   (thePageframeMap[index].flags & PG_RESERVED)

void pg_init(page_frame_t *pf, uint flags) {
    if (flags & PG_RESERVED) {
        pf->flags = flags;
        pf->used = PG_RESERVED_USE;
    }
    else
        ++n_available_pages;
        // pf->used is zeroed already

    current_page = (index_t)(pf - thePageframeMap);
}


#define _pg_release(index)      \
    do {                        \
        -- thePageframeMap[index].used; i                       \
        if (thePageframeMap[i].used == 0) ++n_available_pages;  \
    } while (0)

#define _pg_free(index)     \
    do {  thePageframeMap[index].used = 0; ++n_available_pages; } while (0)

void pg_free(index_t index) {
    _pg_free(index);
}

/***
  *     Global pages catalog
 ***/

void pages_set(uint addr, uint size, bool reserved) {
    uint flags = reserved ? PG_RESERVED : 0;
    uint end_page = aligned(addr + size - 1) / PAGE_SIZE;
    if (end_page == 0) return;

    uint i = addr / PAGE_SIZE;
    if ((!reserved) && (addr % PAGE_SIZE)) ++i;

    mem_logf("%s pages from %x to %x; ", (reserved? "reserving" : "freeing  "),  i, end_page - 1);

    for (; i < end_page; ++i) {
        pg_init(thePageframeMap + i, flags);
    }
    if (reserved) {
        pg_init(thePageframeMap + i , flags);
    } else
        if (n_pages <= end_page) {
            n_pages = end_page;
            logf("n_pages = %x", n_pages);
        }
        else logf("odd thing: n_pages(%d) > end_page(%d)", n_pages, end_page);
    log("\n");
}

void pmem_setup(void) {
    thePageframeMap = (page_frame_t *)aligned((uint)&_end);

    struct memory_map *mapping = (struct memory_map *)mboot_mmap_addr();

    uint i;
    for (i = 0; i < mboot_mmap_length(); ++i) {
        if (mapping[i].length_low == 0) continue;

        /* set thePageframeMap entries */
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
    pg_init(thePageframeMap, true);

    /* reserve kernel and thePageframeMap itself */
    pages_set(__pa(&_start), n_pages * sizeof(page_frame_t), true);

    kheap_setup();
}

/***
  *     Page management
 ***/

uint pg_alloc(void) {
    return 0;
}

index_t pmem_alloc(size_t pages_count) {
    if (pages_count < 1) return 0;
    if (pages_count > n_available_pages) return 0;

#warning "Temporary code: no releasing, a more elaborate algorithm required"
    index_t i = current_page + 1;
    index_t last_reserved = i;
    for (; i < n_pages; ++i) {
        if (pg_is_used(i))
            last_reserved = i;

        if ((i - last_reserved) >= pages_count) {
            int j;
            mem_logf("pmem_alloc(%x) => page[%x]\n", pages_count, last_reserved);
            for (j = 0; j < (int)pages_count; ++j) {
                ++ thePageframeMap[last_reserved + j].used;
                -- n_available_pages;
            }
            current_page = last_reserved + pages_count;
            return last_reserved;
        }
    }
    return 0;
}

err_t pmem_free(index_t start_page, size_t pages_count) {
    index_t i = start_page;
    for (; i < start_page + pages_count; ++i) {
        if (pg_is_reserved(i) || !pg_is_used(i))
            return -1;

        pg_free(i);
    }
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
        if (pg_is_reserved(i))
            ++reserved_pages;
        else
            if (pg_is_used(i)) ++occupied_pages;
            else ++free_pages;
    }
    printf("Page size: 0x%x\nMem map[0x%x] at 0x%x\n", PAGE_SIZE, n_pages, (uint)thePageframeMap);
    printf("Pages count: 0x%x; free: 0x%x(0x%x), occupied: 0x%x, reserved: 0x%xh\n",
            n_pages, n_available_pages, free_pages, occupied_pages, reserved_pages);
    printf("currrent page index = 0x%x\n", current_page);
}

#endif
