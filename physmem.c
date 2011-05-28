#include <physmem.h>
#include <defs.h>
#include <mboot.h>
#include <multiboot.h>

void phmem_setup(void) {
    struct memory_map *mmmap = (struct memory_map *)mboot_mmap_addr();
    uint i;
    for (i = 0; i < mboot_mmap_length(); ++i) {
        if (mmmap[i].length_low == 0) continue;
           
    }
}
