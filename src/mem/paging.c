#include <stdint.h>
#include <string.h>

#include <mem/paging.h>
#include <arch/i386.h>

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
    void * phy_pagedir = __pa(&thePageDirectory);
    // prepare thePageDirectory
    memcpy(thePageDirectory, (void *)INITIAL_PGDIR, N_PDE * sizeof(pde_t));
    // switch
    k_printf("switching page directory to *%x\n", phy_pagedir);
    i386_switch_pagedir(phy_pagedir);
}
