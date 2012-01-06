//#include <core/paging.h>
#include <arch/i386.h>

typedef  uint32_t  pde_t;
typedef  uint32_t  pte_t;

#define PG_PRESENT      0x00000001
#define PG_RW           0x00000002
#define PG_USR_READ     0x00000004
#define PG_PWT          0x00000008
#define PG_PCD          0x00000010
#define PG_ACCESSED     0x00000020
#define PG_DIRTY        0x00000040
#define PG_GRAN         0x00000080  /* page size: Mb/Kb */
#define PG_GLOBL        0x00000100
#define PG_PAT          0x00001000

#define PG31_22_MASK    0xFFC00000
#define PG31_12_MASK    0xFFFFF000

#define PDE_SHIFT       22
#define PTE_SHIFT       12

#define N_PDE           (PAGE_SIZE / sizeof(pde_t))
#define PTE_PER_ENTRY   (PAGE_SIZE / sizeof(pte_t))

pde_t thePageDirectory[N_PDE] __attribute__((aligned (PAGE_SIZE))) = {
    [0] = 0x00000000 | PG_GRAN | PG_RW | PG_PRESENT,
    [1] = 0,
    [KERN_PA >> PDE_SHIFT] = 0x0000000 | PG_GRAN | PG_RW | PG_PRESENT,
};
