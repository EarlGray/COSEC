#ifndef __PHYS_MEM_H__
#define __PHYS_MEM_H__

void phmem_setup(void);

uint pg_alloc(void);
void pg_free(uint);

#endif // __PHYS_MEM_H__
