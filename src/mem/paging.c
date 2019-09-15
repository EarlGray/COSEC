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
    for (size_t i = 0; i < KERN_OFF / 0x400000; ++i) {
        uintptr_t pptr = i * 0x400000;
        bool present = false;

        for (size_t j = 0; j < pmem_map_count; ++j) {
            uint64_t start = ((uint64_t)pmem_maps[j].base_addr_high << 32) + pmem_maps[j].base_addr_low;
            uint64_t len = ((uint64_t)pmem_maps[j].length_high << 32) + pmem_maps[j].length_low;
            if (start >= (pptr + 0x400000))
                continue;
            if ((start + len) <= pptr)
                continue;
            present = true;
            break;
        }

        if (!present) continue;

        pde_t pde = { .word = pptr };
        pde.bit.hugepage = true;
        pde.bit.present = true;
        pde.bit.writable = true;

        thePageDirectory[i] = pde;
    }

    // switch
    void * phy_pagedir = __pa(thePageDirectory);
    k_printf("switching page directory to *%x\n", phy_pagedir);
    i386_switch_pagedir(phy_pagedir);
}
