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
#include <sys/errno.h>

#if MEM_DEBUG
#   define mem_logf(msg, ...) lprintf(msg, __VA_ARGS__)
#else
#   define mem_logf(msg, ...)
#endif

typedef size_t pageindex_t;

#define UPPER_MEMORY_OFFSET         0x100000
#define UPPER_MEMORY_PAGE_OFFSET    (UPPER_MEMORY_OFFSET / PAGE_BYTES)

extern char _end;

pageindex_t theEdgeOfAllocatedMemory;
pageindex_t theEndOfUpperMemory;

/*
 *      Utilities
 */

/*      page_aligned(n*PAGE_BYTES) = n*PAGE_BYTES
 *      page_aligned(n*PAGE_BYTES + 1) = (n+1)*PAGE_BYTES
 */
static inline index_t page_aligned(uintptr_t addr) {
    if (addr % PAGE_BYTES == 0) return addr / PAGE_BYTES;
    return (1 + addr / PAGE_BYTES);
}

static inline index_t page_aligned_back(uintptr_t addr) {
    return (addr / PAGE_BYTES);
}

void pmem_info(void) {
    struct memory_map *mmmap = (struct memory_map *)mboot_mmap_addr();
    size_t upper_memory = mboot_uppermem_kb();
    k_printf("Upper memory = %d MB (0x%x B)\n", upper_memory / 1024, upper_memory * 1024);
    k_printf("Memory map [%d]\n", mboot_mmap_length());
    for (size_t i = 0; i < mboot_mmap_length(); ++i) {
        uint32_t t = mmmap[i].type;
        if (0 < t && t < 10) {
            k_printf("[%d]: type=%d, base=%08x%08x, len=%08x%08x, sh=0x%x\n",
                i, t,
                mmmap[i].base_addr_high, mmmap[i].base_addr_low,
                mmmap[i].length_high, mmmap[i].length_low,
                mmmap[i].size
            );
        }
    }
}

void pmem_setup(void) {
    pageindex_t upper_memory_pages = mboot_uppermem_kb() / (PAGE_BYTES/1024);
    if (upper_memory_pages >= 1024 * 1024) {
        upper_memory_pages = (1024 * 1024) - 1;
        k_printf("Using only 4096 MB RAM out of %d MB\n",
                upper_memory_pages / (1024 * 1024 / PAGE_BYTES));
    }

    theEndOfUpperMemory = (UPPER_MEMORY_OFFSET / PAGE_BYTES) + upper_memory_pages;

    uintptr_t free_pmem_edge = (uintptr_t)__pa(&_end);

    // Skip multiboot modules for now.
    // Usually they are placed right after the kernel _end.
    module_t *mods = NULL;
    size_t n_mods = 0;
    mboot_modules_info(&n_mods, &mods);
    for (size_t i = 0; i < n_mods; ++i) {
        if (mods[i].mod_end > free_pmem_edge) {
            free_pmem_edge = mods[i].mod_end;
        }
    }

    theEdgeOfAllocatedMemory = page_aligned(free_pmem_edge);

    // TODO: rescue the mboot structures.
    // TODO: create the freed regions list.
    // TODO: add the lower memory to the list of freed pages.
}

void * pmem_alloc(size_t pages_count) {
    pageindex_t old_edge = theEdgeOfAllocatedMemory;
    pageindex_t new_edge = old_edge + pages_count;

    if (new_edge > theEndOfUpperMemory) {
        // TODO: fall back to the freed list.
        return 0;
    }

    logmsgdf("%s(0x%x) -> *%08x\n", __func__, pages_count, PAGE_BYTES * old_edge);
    theEdgeOfAllocatedMemory = new_edge;
    return (void *)(PAGE_BYTES * old_edge);
}

err_t pmem_free(index_t start_page, size_t pages_count) {
    /* TODO: adding the region to the freed list */
    /* TODO: merging it with adjacent regions */
    if (start_page + pages_count == theEdgeOfAllocatedMemory) {
        // bump back:
        theEdgeOfAllocatedMemory = start_page;
        return 0;
    } else {
        logmsgf("%s(0x%x, len=%d) : TODO\n", __func__, start_page, pages_count);
        return ETODO;
    }
}


/*
 *  FS character memory device 1:1, /dev/mem
 */

static const char *chrram_mem_get_roblock(device *cmem, off_t block) {
    UNUSED(cmem);
    size_t addr = block * PAGE_BYTES;
    return (const char *)addr;
}

static char *chrram_mem_get_rwblock(device *cmem, off_t block) {
    UNUSED(cmem);
    size_t addr = block * PAGE_BYTES;
    return (char *)addr;
}

static size_t chrram_mem_blocksize(device *cmem) {
    UNUSED(cmem);
    return PAGE_BYTES;
}

static off_t chrram_mem_size(device *cmem) {
    UNUSED(cmem);
    off_t msize = mboot_uppermem_kb();
    ulong usize = ((ulong)msize * 1024) / PAGE_BYTES;
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


void memory_setup(void) {
    pmem_setup();
#if PAGING
    paging_setup();
#endif
    kheap_setup();
}
