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
static inline off_t page_aligned(off_t addr) {
    if (addr % PAGE_SIZE == 0) return addr / PAGE_SIZE;
    return (1 + addr / PAGE_SIZE);
}

static inline off_t page_aligned_back(off_t addr) {
    return (addr / PAGE_SIZE);
}

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
   void *owner;                     // virtual memory manager/cache manager
   index_t vm_offset;               // index in vma
} pageframe_t;


/***
  *     Page lists
 ***/

#define PF_LIST_INSERT(list, frame) do {\
    (list)->head->prev->next = (frame); \
    (frame)->next = (list)->head;       \
    (frame)->prev = (list)->head->prev; \
    (list)->head->prev = (frame);   \
    (list)->count++;                \
    } while (0)

#define PF_LIST_REMOVE(list, node) do { \
    if ((list)->head == (node))         \
        if ((node)->next != (node)) {   \
            (node)->next->prev = (node)->prev;  \
            (node)->prev->next = (node)->next;  \
            (list)->head = (node)->next;        \
        } else panic("Removing the last node"); \
    else {  \
        (node)->prev->next = (node)->next;    \
        (node)->next->prev = (node)->prev;    \
    }; \
    (list)->count--; } while (0)

#define PF_LIST_INIT(list, node) \
    do {   \
     (list)->head = (node); \
     (list)->count = 1;     \
     (node)->next = (node); \
     (node)->prev = (node); \
    } while (0)

typedef struct pagelist {
    pageframe_t *head;
    size_t count;
} pagelist_t;



// the global pageframe table:
pageframe_t *the_pageframe_map = null;
size_t      pfmap_len = 0;

pagelist_t free_pageframes = { null, 0 };
pagelist_t used_pageframes = { null, 0 };
pagelist_t cache_pageframes = { null, 0 };
pagelist_t reserved_pageframes = { null, 0 };

void pmem_setup(void) {
    int i;
    struct memory_map *mapping = (struct memory_map *)mboot_mmap_addr();
    size_t mmap_len = mboot_mmap_length();
    ptr_t pfmap = (ptr_t)&_end;

    // switch page directory to in-kernel one
    paging_setup();

    /// determine pfmap_len
    for (i = 0; i < mmap_len; ++i) {
        struct memory_map *m = mapping + i;
        ptr_t map_end = page_aligned(m->base_addr_low + m->length_low);
        if ((m->type == 1) && (map_end > pfmap_len))  pfmap_len = map_end;
    }

    /// allocate the pageframe map
    // skip all multiboot modules
    module_t *mods = null;
    size_t n_mods = 0;
    mboot_modules_info(&n_mods, &mods);
    for (i = 0; i < n_mods; ++i) {
        ptr_t mod_end = __va(mods[i].mod_end);
        if (pfmap < mod_end) pfmap = mod_end;
    }

    pfmap = PAGE_SIZE * page_aligned(pfmap);
    ptr_t pfmap_end = pfmap + sizeof(pageframe_t) * pfmap_len;
    mem_logf("pfmap at *%x:*%x (0x%x pageframes)\n", pfmap, pfmap_end, pfmap_len);

    // allocate virtual 4M pages
    if (pfmap_end > (ptr_t)__va(0x400000)) {
        panic("TODO: allocate memory for pageframe table more than 4Mb long\n");
    }
    the_pageframe_map = (pageframe_t *)pfmap;
    memset(the_pageframe_map, 0, pfmap_len * sizeof(pageframe_t));

    // consider all memory reserved first
    PF_LIST_INIT(&reserved_pageframes, &(the_pageframe_map[0]));
    reserved_pageframes.head->flags |= PF_RESERVED;

    for (i = 1; i < pfmap_len; ++i) {
        pageframe_t *pf = the_pageframe_map + i;
        PF_LIST_INSERT(&reserved_pageframes, pf);
        pf->flags = PF_RESERVED;
    }

    // free the free regions
    int mbm;
    for (mbm = 0; mbm < mmap_len; ++mbm)
        if (mapping[mbm].type == 1) {
            index_t start = page_aligned(mapping[mbm].base_addr_low);
            index_t end = page_aligned(mapping[mbm].base_addr_low + mapping[mbm].length_low);
            mem_logf("marking free [%x : %x)\n", start, end);
            for (i = start; i < end; ++i) {
                pageframe_t *pf = the_pageframe_map + i;
                PF_LIST_REMOVE(&reserved_pageframes, pf);
                if (free_pageframes.head)
                    PF_LIST_INSERT(&free_pageframes, pf);
                else PF_LIST_INIT(&free_pageframes, pf);
                pf->flags = PF_FREE;
            }
        } // */

    mem_logf("free: %x, reserved; %x\n",
            free_pageframes.count, reserved_pageframes.count);
}


index_t pmem_alloc(size_t pages_count) {
    return 0;
}

void pmem_info(void) {
    struct memory_map *mmmap = (struct memory_map *)mboot_mmap_addr();
    uint i;
    printf("\nMemory map [%d]\n", mboot_mmap_length());
    for (i = 0; i < mboot_mmap_length(); ++i) {
        if (mmmap[i].type == 0) continue;
        if (mmmap[i].size > 100) continue;

        printf("%d: %d) base=\t%x\t%x,len=%x\t%x,sh=%x\n",
            i, mmmap[i].type,
            mmmap[i].base_addr_high, mmmap[i].base_addr_low,
            mmmap[i].length_high, mmmap[i].length_low,
            mmmap[i].size
        );
    }
}

