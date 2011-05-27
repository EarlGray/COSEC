#ifndef __MBOOT_H__
#define __MBOOT_H__

#include <defs.h>

struct multiboot_info;

const char * mboot_cmdline(void); 
uint32_t    mboot_mmap_length(void);
void *      mboot_mmap_addr(void);
uint32_t    mboot_drives_length(void);

void mboot_info_parse(struct multiboot_info *mbi);

#endif //__MBOOT_H__
