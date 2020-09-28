#ifndef __PAGING_H__
#define __PAGING_H__

#include <conf.h>

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

#define CR0_PG      0x80000000
#define CR0_WP      0x00010000

#define KERN_STACK_SIZE  0x00001000


#ifndef NOT_CC

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

static inline void *__pa(void *vaddr) {
    uintptr_t p = (uintptr_t)vaddr;
    p -= KERN_OFF;
    return (void *)p;
}

static inline void *__va(void *paddr) {
    uintptr_t p = (uintptr_t)paddr;
    p += KERN_OFF;
    return (void *)p;
}

enum pte_bits {
    PTE_PRESENT = 1 << 0,
    PTE_WRITABLE = 1 << 1,
    PTE_USER = 1 << 2,
};

typedef union {
    uint32_t word;
    struct {
        bool present:1;
        bool writable:1;
        bool user:1;
        bool writethrough:1;
        bool dontcache:1;
        bool accessed:1;
        bool reserved:1;
        bool hugepage:1;
        bool ignored:1;
        uint8_t avail:3;
        uint32_t index:20;
    } bit;
} pde_t;

typedef union {
    uint32_t word;
    struct {
        bool present:1;
        bool writable:1;
        bool user:1;
        bool writethrough:1;
        bool dontcache:1;
        bool accessed:1;
        bool dirty:1;
        bool reserved:1;
        bool global:1;
        uint8_t avail:3;
        uint32_t index:20;
    } bit;
} pte_t;

extern pde_t thePageDirectory[N_PDE];

void pg_fault(uint32_t *context, err_t err);

void paging_setup(void);

pde_t * pagedir_alloc(void);
void pagedir_free(pde_t *pagedir);

void* pagedir_get_or_new(pde_t *pagedir, void *vaddr, uint32_t pte_mask);

#endif // NOT_CC
#endif //__PAGING_H__
