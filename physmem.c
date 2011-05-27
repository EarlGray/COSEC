#include <physmem.h>
#include <defs.h>
#include <mboot.h>
#include <multiboot.h>

void phmem_setup(void) {
    struct memory_map *mmmap = (struct memory_map *)mboot_mmap_addr();
    uint i;
    k_printf("\nMemory map [%d]\n", mboot_mmap_length());
    for (i = 0; i < mboot_mmap_length(); ++i) {
#ifdef VERBOSE
        if (mmmap[i].length_low == 0) continue;
        k_printf("%d) sz=%x,bl=%x,bh=%x,ll=%x,lh=%x,t=%x\n", i, mmmap[i].size, 
               mmmap[i].base_addr_low, mmmap[i].base_addr_high,
               mmmap[i].length_low, mmmap[i].length_high, 
               mmmap[i].type);
#endif
    }
}
