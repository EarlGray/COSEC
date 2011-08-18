//#include <core/paging.h>
#include <arch/i386.h>

#define PDE_COUNT   1024
#define PTE_COUNT   1024

typedef  uint  pde_t;
typedef  uint  pte_t;

pde_t thePageDirectory[PDE_COUNT] __attribute__((aligned (PAGE_SIZE)));
pte_t first_page_table[PTE_COUNT] __attribute__((aligned (PAGE_SIZE)));

