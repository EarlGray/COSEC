#ifndef __PHYS_MEM_H__
#define __PHYS_MEM_H__

#define VIRTUAL_ADDRESS 0xc0100000

uint pg_alloc(void);
void pg_free(uint);

/***
  *     Allocating pages
  *     returns number of first page
 ***/
uint pmem_alloc(size_t pages_count);

void pmem_setup(void);
void pmem_info(void);

#endif // __PHYS_MEM_H__
