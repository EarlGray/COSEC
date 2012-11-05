#ifndef __COSEC_ELF__H__
#define __COSEC_ELF__H__

#include <std/sys/elf.h>

void print_section_headers(Elf32_Shdr *shdr, size_t snum);
void print_elf_syms(Elf32_Sym *syms, size_t n_syms, const char *strtab, const char *symname);

Elf32_Shdr *elf_section_by_name(Elf32_Shdr *shdr, size_t snum, const char *sname);

#endif //__COSEC_ELF__H__
