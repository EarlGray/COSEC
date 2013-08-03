/*
 *   This file represents physical memory "object"
 * and its methods.
 *   Physical memory is treated as a heap of pageframes.
 *   This is reflected in thePageframeMap, which resides right after
 * the kernel code: it is array of page_frame structures with
 * length n_pages;
 */
#include <mem/pmem.h>

#include <mem/kheap.h>
#include <mem/paging.h>

#include <arch/i386.h>
#include <arch/mboot.h>

#include <log.h>

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

typedef struct pagelist {
    pageframe_t *head;
    size_t count;
    uint flag;
} pagelist_t;


#define PF_LIST_INSERT(list, frame) do {\
    (list)->head->prev->next = (frame); \
    (frame)->next = (list)->head;       \
    (frame)->prev = (list)->head->prev; \
    (list)->head->prev = (frame);   \
    (list)->count++;                \
    } while (0)

inline static void pf_list_insert(pagelist_t *list, pageframe_t *pf) {
    list->head->prev->next = pf;
    pf->next = list->head;
    pf->prev = list->head->prev;
    pf->flags = list->flag;
    list->head->prev = pf;
    list->count++;
}

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

inline static void pf_list_remove(pagelist_t *list, pageframe_t *node) {
    if (list->head == node)
        if (node->next != node) {
            node->next->prev = node->prev;
            node->prev->next = node->next;
            list->head = node->next;
        } 
        else panic("Removing the last node");
    else {
        node->prev->next = node->next;
        node->next->prev = node->prev;
    };
    list->count--;
}

#define PF_LIST_INIT(list, node) \
    do {   \
     (list)->head = (node); \
     (list)->count = 1;     \
     (node)->next = (node); \
     (node)->prev = (node); \
    } while (0)

inline static void pf_list_init(pagelist_t *list, pageframe_t *node, uint flag) {
     list->head = node;
     list->count = 1;
     list->flag = flag;
     node->next = node;
     node->prev = node;
     node->flags = flag;
}



// the global pageframe table:
pageframe_t *the_pageframe_map = null;
size_t      pfmap_len = 0;

pagelist_t free_pageframes = { null, 0 };
pagelist_t used_pageframes = { null, 0 };
pagelist_t cache_pageframes = { null, 0 };
pagelist_t reserved_pageframes = { null, 0 };


inline static index_t pageframe_index(pageframe_t *pf) {
    return pf - the_pageframe_map;
}

void pmem_setup(void) {
    int i;
    struct memory_map *mapping = (struct memory_map *)mboot_mmap_addr();
    size_t mmap_len = mboot_mmap_length();
    ptr_t pfmap = (ptr_t)&_end;

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
    pf_list_init(&reserved_pageframes, &(the_pageframe_map[0]), PF_RESERVED);

    for (i = 1; i < pfmap_len; ++i) {
        pageframe_t *pf = the_pageframe_map + i;
        pf_list_insert(&reserved_pageframes, pf);
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
                pf_list_remove(&reserved_pageframes, pf);
                if (free_pageframes.head)
                    pf_list_insert(&free_pageframes, pf);
                else pf_list_init(&free_pageframes, pf, PF_FREE);
            }
        } // */

    // mark the first page as reserved
    mem_logf("marking reserved page %d\n", 0);
    pageframe_t *pf = the_pageframe_map + 0;
    pf_list_remove(&free_pageframes, pf);
    pf_list_insert(&reserved_pageframes, pf);

    // mark the last free pageframe as cache
    pageframe_t * cpf = free_pageframes.head->prev;
    pf_list_init(&cache_pageframes, cpf, PF_CACHE);
    pf_list_remove(&free_pageframes, cpf);
    mem_logf("marking cache %x\n", pageframe_index(cpf));

    mem_logf("free: %x, reserved: %x, cache: %x\n",
            free_pageframes.count, reserved_pageframes.count, cache_pageframes.count);
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

