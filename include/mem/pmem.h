#ifndef __PHYS_MEM_H__
#define __PHYS_MEM_H__

#include <stdbool.h>
#include <stdint.h>

#include "mem/paging.h"

/** allocate a page **/
index_t pg_alloc(void);

/** free the page **/
void pg_free(index_t);


/*
 *  Allocate consequent pageframes,
 *  returns physical address.
 */
void * pmem_alloc(size_t pages_count);

static inline void * kmem_alloc(size_t pages_count) {
    void *p = pmem_alloc(pages_count);
    return __va(p);
}

err_t pmem_reserve(void *startptr, void *endptr);

/* returns 0 if all pages are available,
 * or index of the first unavailable page */
index_t pmem_check_avail(void *startptr, void *endptr);

/*
 *  Releases pages
 */

err_t pmem_free(index_t start_page, size_t pages_count);

void pmem_setup(void);
void pmem_info(void);


#define VM_RW       (1 << 1)
#define VM_USR      (1 << 2)
#define VM_XD       (uint64_t)(1 << 63)

typedef struct vm_area vm_area_t;

void memory_setup(void);

struct device;
struct device * get_cmem_device(void);

#endif // __PHYS_MEM_H__
