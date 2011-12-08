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
index_t pmem_alloc(size_t pages_count);

/*
 *  Releases pages
 */

err_t pmem_free(index_t start_page, size_t pages_count);

void pmem_setup(void);
void pmem_info(void);

#endif // __PHYS_MEM_H__
