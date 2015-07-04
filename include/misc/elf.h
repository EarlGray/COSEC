#ifndef __COSEC_ELF_H__
#define __COSEC_ELF_H__

#include <linux/elf.h>

Elf32_Shdr *elf_section_by_name(Elf32_Shdr *shdr, size_t snum, const char *sname);
void print_elf_syms(Elf32_Sym *syms, size_t n_syms, const char *strtab, const char *symname);
void print_section_headers(Elf32_Shdr *shdr, size_t snum);

#endif //__COSEC_ELF_H__
