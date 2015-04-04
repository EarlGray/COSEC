/*
 *   This file represents physical memory "object"
 * and its methods.
 *   Physical memory is treated as a heap of pageframes.
 *   This is reflected in the_pageframe_map, which resides right after
 * the kernel code: it is array of page_frame structures with
 * length n_pages;
 */
#include <mem/pmem.h>

#include <mem/kheap.h>
#include <mem/paging.h>

#include <arch/i386.h>
#include <arch/mboot.h>

#include <fs/devices.h>

#include <string.h>
#include <stdbool.h>

#include <cosec/log.h>

#if MEM_DEBUG
#   define mem_logf(msg, ...) k_printf(msg, __VA_ARGS__)
#else
#   define mem_logf(msg, ...)
#endif

extern char _start, _end;

/***
  *     Alignment
 ***/

/*      aligned(n*PAGE_SIZE) = n*PAGE_SIZE
 *      aligned(n*PAGE_SIZE + 1) = (n+1)*PAGE_SIZE
 */
static inline index_t page_aligned(ptr_t addr) {
    if (addr % PAGE_SIZE == 0) return addr / PAGE_SIZE;
    return (1 + addr / PAGE_SIZE);
}

static inline index_t page_aligned_back(ptr_t addr) {
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
size_t      pfmap_len = 0;      // in pageframe_t

pagelist_t free_pageframes;
pagelist_t used_pageframes;
pagelist_t cache_pageframes;
pagelist_t reserved_pageframes;


inline static index_t pageframe_index(pageframe_t *pf) {
    return pf - the_pageframe_map;
}

void * pageframe_addr(pageframe_t *pf) {
    return (void *)(PAGE_SIZE * pageframe_index(pf));
}

static void mark_used(void *p1, void *p2) {
    index_t pft1 = page_aligned_back((ptr_t)p1);
    index_t pft2 = page_aligned((ptr_t)p2);
    index_t i;
    for (i = pft1; i < pft2; ++i) {
        pageframe_t *pf = the_pageframe_map + i;
        if (pf->flags != PF_FREE) {
            logmsgef("pmem_init: trying to mark page #%x as used, its flags are %d\n",
                    i, pf->flags);
            return;
        }

        pf_list_remove(&free_pageframes, pf);
        pf_list_insert(&used_pageframes, pf);
    }
}

void pmem_setup(void) {
    index_t i;
    struct memory_map *mapping = (struct memory_map *)mboot_mmap_addr();
    size_t mmap_len = mboot_mmap_length();
    ptr_t pfmap = (ptr_t)&_end;

    /// determine pfmap_len
    for (i = 0; i < mmap_len; ++i) {
        struct memory_map *m = mapping + i;
        index_t map_end = page_aligned(m->base_addr_low + m->length_low);
        if ((m->type == 1) && (map_end > pfmap_len))  pfmap_len = map_end;
    }

    /// allocate the pageframe map
    // skip all multiboot modules
    module_t *mods = null;
    size_t n_mods = 0;
    mboot_modules_info(&n_mods, &mods);
    for (i = 0; i < n_mods; ++i) {
        ptr_t mod_end = (ptr_t)__va(mods[i].mod_end);
        if (pfmap < mod_end) 
            pfmap = mod_end;
    }

    pfmap = PAGE_SIZE * page_aligned(pfmap);
    ptr_t pfmap_end = pfmap + sizeof(pageframe_t) * pfmap_len;
    mem_logf("thePFMap[%x] at *%x (until *%x)\n", pfmap_len, pfmap, pfmap_end);

    // allocate virtual 4M pages
    if (pfmap_end > (ptr_t)__va(0x800000)) {
        panic("TODO: allocate memory for pageframe table more than 8Mb long\n");
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
    size_t mbm;
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

    // mark kernel code&data space as used
    pf = the_pageframe_map + (page_aligned_back((ptr_t)&_start));
    pf_list_init(&used_pageframes, pf, PF_USED);
    mark_used(pageframe_addr(++pf), &_end);

    // mark the multiboot modules memory as used
    for (i = 0; i < n_mods; ++i) {
        mark_used(__va(mods[i].mod_start), __va(mods[i].mod_end));
    }

    // mark the pageframe table memory as used
    mark_used(the_pageframe_map, the_pageframe_map + pfmap_len);

    mem_logf("free: %x, reserved: %x, cache: %x\n",
            free_pageframes.count, reserved_pageframes.count, cache_pageframes.count);
}


void * pmem_alloc(size_t pages_count) {
    if (free_pageframes.count == 0)
        return 0;

    /* TODO: the algorithm is stupid and unreliable, rewrite to buddy allocator */
    pageframe_t *startp = free_pageframes.head;
    pageframe_t *currentp = startp;

    while (true) {
        bool available = true;
        size_t i;
        for (i = 0; i < pages_count; ++i) {
            currentp = startp + i;

            if (pageframe_index(currentp) >= pfmap_len)
                return 0;

            if (currentp->flags != PF_FREE) {
                available = false;
                break;
            }
        }

        if (available) {
            /* mark those pages used and return */
            for (i = 0; i < pages_count; ++i) {
                pf_list_remove(&free_pageframes, startp + i);
                pf_list_insert(&used_pageframes, startp + i);
            }
            return pageframe_addr(startp);
        }

        // find next free pageframe
        startp = startp + i;
        while (startp->flags != PF_FREE) {
            startp++;

            if (pageframe_index(startp) >= pfmap_len)
                return 0;
        }
    }
}

err_t pmem_free(index_t start_page, size_t pages_count) {
    const char *funcname = __FUNCTION__;
    logmsgf("%s(*%x[%d]) : TODO\n", funcname, (uint)start_page, pages_count);
    return 0;
}

void pmem_info(void) {
    struct memory_map *mmmap = (struct memory_map *)mboot_mmap_addr();
    uint i;
    k_printf("\nMemory map [%d]\n", mboot_mmap_length());
    for (i = 0; i < mboot_mmap_length(); ++i) {
        if (mmmap[i].type == 0) continue;
        if (mmmap[i].size > 100) continue;

        k_printf("%d: %d) base=\t%x\t%x,len=%x\t%x,sh=%x\n",
            i, mmmap[i].type,
            mmmap[i].base_addr_high, mmmap[i].base_addr_low,
            mmmap[i].length_high, mmmap[i].length_low,
            mmmap[i].size
        );
    }
}


/*
 *  FS character memory device 1:1, /dev/mem
 */

static const char *chrram_mem_get_roblock(device *cmem, off_t block) {
    UNUSED(cmem);
    size_t addr = block * PAGE_SIZE;
    return (const char *)addr;
}

static char *chrram_mem_get_rwblock(device *cmem, off_t block) {
    UNUSED(cmem);
    size_t addr = block * PAGE_SIZE;
    return (char *)addr;
}

static size_t chrram_mem_blocksize(device *cmem) {
    UNUSED(cmem);
    return PAGE_SIZE;
}

static off_t chrram_mem_size(device *cmem) {
    UNUSED(cmem);
    off_t msize = mboot_uppermem_kb();
    ulong usize = ((ulong)msize * 1024) / PAGE_SIZE;
    /* TODO: check for off_t boundary */
    msize = (off_t)usize;
    return msize;
}

static int chrram_read_buf(device *cmem, char *buf, size_t buflen, size_t *written, off_t pos) {
    UNUSED(cmem);
    /* TODO: check memory boundaries */
    memcpy(buf, (void *)pos, buflen);

    if (written) *written = buflen;
    return 0;
}
static int chrram_write_buf(device *cmem, const char *buf, size_t buflen, size_t *written, off_t pos) {
    UNUSED(cmem);
    /* TODO: check memory boundaries */
    memcpy((void *)pos, buf, buflen);

    if (written) *written = buflen;
    return 0;
}

struct device_operations chrram_mem_ops = {
    .dev_get_roblock    = chrram_mem_get_roblock,
    .dev_get_rwblock    = chrram_mem_get_rwblock,
    .dev_forget_block   = NULL,
    .dev_size_of_block  = chrram_mem_blocksize,
    .dev_size_in_blocks = chrram_mem_size,

    .dev_read_buf   = chrram_read_buf,
    .dev_write_buf  = chrram_write_buf,
    .dev_has_data   = NULL,
};

struct device  chrram_mem = {
    .dev_type       = DEV_CHR,
    .dev_clss       = CHR_VIRT,
    .dev_no         = CHRMEM_MEM,

    .dev_ops        = &chrram_mem_ops,
};

device * get_cmem_device(void) {
    return &chrram_mem;
}


void vmem_setup(void) {
}

void memory_setup(void) {
#if PAGING
    paging_setup();
#endif
    pmem_setup();
    vmem_setup();
    kheap_setup();
}
