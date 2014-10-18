#define __DEBUG

#include <stdint.h>
#include <string.h>

#include <mem/paging.h>
#include <arch/i386.h>
#include <log.h>

pde_t thePageDirectory[N_PDE] __attribute__((aligned (PAGE_SIZE)));

int pg_fault(void) {
    print("\n#PF\n");

    ptr_t fault_addr;
    asm volatile ("movl %%cr2, %0   \n" : "=r"(fault_addr) );

    err_t fault_error = intr_err_code();
    ptr_t context = intr_context_esp();

    uint* op_addr = (uint *)(context + CONTEXT_SIZE + sizeof(uint));

    printf("Fault 0x%x from %x:%x accessing *%x\n",
            fault_error, op_addr[1], op_addr[0], fault_addr);

    cpu_hang();
}

void paging_setup(void) {
    void * phy_pagedir = __pa(&thePageDirectory);
    // prepare thePageDirectory
    memcpy(thePageDirectory, (void *)INITIAL_PGDIR, N_PDE * sizeof(pde_t));
    // switch
    k_printf("switching page directory to *0x%x\n", phy_pagedir);
    i386_switch_pagedir(phy_pagedir);
}
