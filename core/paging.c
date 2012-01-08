#define __DEBUG

#include <mm/paging.h>
#include <arch/i386.h>
#include <log.h>

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
