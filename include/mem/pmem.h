#ifndef __PHYS_MEM_H__
#define __PHYS_MEM_H__

#define VIRTUAL_ADDRESS 0xc0100000

/** allocate a page **/
index_t pg_alloc(void);

/** free the page **/
void pg_free(index_t);

/*
 *  Allocate consequent pageframes
 *  returns index of first page
 */
void * pmem_alloc(size_t pages_count);

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

#endif // __PHYS_MEM_H__
