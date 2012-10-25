#ifndef __COSEC_ELF__H__
#define __COSEC_ELF__H__

typedef struct Elf32_Shdr Elf32_Shdr;

void print_section_headers(Elf32_Shdr *shdr, size_t snum);

#endif //__COSEC_ELF__H__
