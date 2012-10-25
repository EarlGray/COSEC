#ifndef __MBOOT_H__
#define __MBOOT_H__

#include <arch/multiboot.h>

const char * boot_cmdline(void); 
uint32_t    boot_mmap_length(void);
void *      boot_mmap_addr(void);
uint32_t    boot_drives_length(void);

void mboot_info_parse(struct multiboot_info *mbi);
void mboot_modules_info(count_t *, module_t **);
elf_section_header_table_t* mboot_kernel_shdr(void);

#endif //__MBOOT_H__
