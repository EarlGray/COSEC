#ifndef __MBOOT_H__
#define __MBOOT_H__

#include <stdint.h>
#include <arch/multiboot.h>

const char *mboot_cmdline(void); 
uint32_t    mboot_mmap_length(void);
void *      mboot_mmap_addr(void);
uint32_t    mboot_drives_length(void);

void mboot_info_parse(struct multiboot_info *mbi);
void mboot_modules_info(count_t *, module_t **);
elf_section_header_table_t* mboot_kernel_shdr(void);
void print_mboot_info(void);

#endif //__MBOOT_H__
