#include "conf.h"
#include "mem/pmem.h"
#include <stdint.h>
#include <string.h>

#include <mem/paging.h>
#include <arch/i386.h>
#include <arch/mboot.h>

#define __DEBUG
#include <cosec/log.h>

pde_t thePageDirectory[N_PDE] __attribute__((aligned (PAGE_SIZE)));

void pg_fault(uint32_t *context, err_t err) {
    uintptr_t fault_addr;
    asm volatile ("movl %%cr2, %0   \n" : "=r"(fault_addr) );

    err_t fault_error = context[0];
    uint32_t eip = context[1];
    uint32_t cs = context[2];

    logmsgef("Page Fault: err=0x%x from %x:%x accessing *%x\n",
             fault_error, cs, eip, fault_addr);

    cpu_hang();
}

void paging_setup(void) {
    // prepare thePageDirectory
    memcpy(thePageDirectory, (void *)INITIAL_PGDIR, N_PDE * sizeof(pde_t));

    // make all physical memory identity-mapped until KERN_OFF
    size_t pmem_map_count = mboot_mmap_length();
    memory_map_t *pmem_maps = mboot_mmap_addr();
    for (size_t i = 0; i < (KERN_OFF >> PDE_SHIFT); ++i) {
        uintptr_t pptr = i * 0x400000;
        bool present = false;
        bool cache_ok = true;

        for (size_t j = 0; j < pmem_map_count; ++j) {
            uint64_t start = ((uint64_t)pmem_maps[j].base_addr_high << 32) + pmem_maps[j].base_addr_low;
            uint64_t len = ((uint64_t)pmem_maps[j].length_high << 32) + pmem_maps[j].length_low;
            if (start >= (pptr + 0x400000))
                continue;
            if ((start + len) <= pptr)
                continue;
            present = true;
            if (pmem_maps[j].type != 1)
                cache_ok = false;
            break;
        }

        if (!present) continue;

        pde_t pde = { .word = pptr };
        pde.bit.hugepage = true;
        pde.bit.present = true;
        pde.bit.writable = true;
        if (!cache_ok) {
            pde.bit.dontcache = true;
        }

        thePageDirectory[i] = pde;
        size_t ik = i + (KERN_OFF >> PDE_SHIFT);
        if (ik < N_PDE)
            thePageDirectory[ik] = pde;
    }

    // switch
    void * phy_pagedir = __pa(thePageDirectory);
    k_printf("switching page directory to *%x\n", phy_pagedir);
    i386_switch_pagedir(phy_pagedir);
}

/* returns physical address */
pde_t * pagedir_alloc(void) {
    void *pagedir = pmem_alloc(1);      // phys. addr.
    pde_t *vpde = __va(pagedir);
    memset(vpde, 0, PAGE_SIZE);

    /* set up kernel memory */
    size_t kpde_offset = (KERN_OFF >> PDE_SHIFT);
    size_t kpde_size = sizeof(pde_t) * (N_PDE - kpde_offset);
    logmsgdf("%s: to=*%x, from=*%x, kpde_size=0x%x", __func__, vpde+kpde_offset, thePageDirectory+kpde_offset, kpde_size);
    memcpy(vpde + kpde_offset, thePageDirectory + kpde_offset, kpde_size);

    return pagedir;
}

void pagedir_free(pde_t *pagedir) {
    logmsgef("%s(*%x): TODO", __func__, (size_t)pagedir);
}

/*
 * allocates a pageframe,
 * adds it to the page directory at vaddr
 * returns its physical address
 */
void* pagedir_get_or_new(pde_t *pagedir, void *vaddr, uint32_t pte_mask) {
    assert((uintptr_t)vaddr < KERN_OFF, NULL,
            "%s: cannot allocate kernel memory at *%x", __func__, vaddr);

    const uint32_t pte_index = ((uint32_t)vaddr >> PTE_SHIFT) & 0x3ff;
    const uint32_t pde_index = (uint32_t)vaddr >> PDE_SHIFT;
    logmsgdf("%s(*%x): pde=0x%x, pte=0x%x\n", __func__, vaddr, pde_index, pte_index);

    pde_t *vpde = __va(pagedir);
    pde_t pde = vpde[pde_index];

    pte_t *pagetbl;
    pte_t *vpte;

    if (pde.word) {
        pagetbl = (pte_t*)(pde.bit.index << PTE_SHIFT);
        vpte = __va(pagetbl);
    } else {
        // request a new pte
        pte_t *pagetbl = pmem_alloc(1);
        assert(pagetbl, NULL, "%s: cannot allocate a PTE", __func__);
        logmsgdf("%s(*%x): new PTE at @%x\n", __func__, vaddr, pagetbl);

        vpte = __va(pagetbl);
        memset(vpte, 0, PAGE_SIZE);

        pde.bit.present = 1;
        pde.bit.writable = 1;
        pde.bit.user = 1;
        pde.bit.writethrough = 1;
        pde.bit.dontcache = 1;
        pde.bit.index = (uint32_t)pagetbl >> 12;

        vpde[pde_index] = pde;
    }

    pte_t pte = vpte[pte_index];
    if (!pte.word) {
        void *page = pmem_alloc(1);
        assert(page, NULL, "%s: cannot allocate a page", __func__);
        logmsgdf("%s(*%x): new page at @%x\n", __func__, vaddr, page);

        pte.word = pte_mask;
        pte.bit.present = 1;
        pte.bit.user = 1;
        pte.bit.index = (uint32_t)page >> 12;

        vpte[pte_index] = pte;
    }

    return (void *)(pte.word & 0xFFFFF000);
}
