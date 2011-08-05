#ifndef __PHYS_MEM_H__
#define __PHYS_MEM_H__

void pmem_setup(void);

uint pg_alloc(void);
void pg_free(uint);

void pmem_info(void);

#endif // __PHYS_MEM_H__
