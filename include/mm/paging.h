#ifndef __PAGING_H__
#define __PAGING_H__

#define INITIAL_PGDIR   0x00001000

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

#define PDE_SIZE        4
#define PTE_SIZE        4

#define N_PDE           (PAGE_SIZE / PDE_SIZE)
#define PTE_PER_ENTRY   (PAGE_SIZE / PTE_SIZE)

#ifndef NOT_CC

#define __pa(vaddr) ((vaddr) - KERN_OFF)
#define __va(paddr) ((paddr) + KERN_OFF)

typedef  uint32_t  pde_t;
typedef  uint32_t  pte_t;

extern pde_t thePageDirectory[N_PDE];

int pg_fault(void);

typedef struct {

} vm_area_t;

#endif // NOT_CC
#endif //__PAGING_H__
