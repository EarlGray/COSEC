#include <std/string.h>
#include <std/sys/elf.h>
#include <log.h>

const char* str_shdr_type[] = {
    [SHT_NULL] =        "NULL",
    [SHT_PROGBITS] =    "PROGBIT",
    [SHT_SYMTAB] =      "SYMTAB",
    [SHT_STRTAB] =      "STRTAB",
    [SHT_RELA] =        "RELA",
    [SHT_HASH] =        "HASH",
    [SHT_DYNAMIC] =     "DYNAMIC",
    [SHT_NOTE] =        "NOTE",
    [SHT_NOBITS] =      "NOBITS",
    [SHT_REL] =         "REL",
    [SHT_SHLIB] =       "SHLIB",
};

void print_section_headers(Elf32_Shdr *shdr, size_t snum) {
    Elf32_Shdr *section;
    const char *shstrtab = null;
    int i;

    // find .shstrtab first 
    for (i = 0; i < snum; ++i) {
        section = shdr + i;
        if (section->sh_type == SHT_STRTAB) {
            shstrtab = section->sh_addr;
            if (!strncmp(".shstrtab", shstrtab + section->sh_name, 9)) break;
            else shstrtab = null;
        }
    }

    assertv(shstrtab, "shstrtab not found");
    k_printf("shstrtab found at *%x\n", (ptr_t) shstrtab);

    for (i = 0; i < snum; ++i) {
        section = shdr + i;
        // type
        uint shtype = section->sh_type;
        if (shtype < sizeof(str_shdr_type)/sizeof(ptr_t))
            k_printf("%s\t", str_shdr_type[shtype]);
        else k_printf("?%d\t", shtype);
    
        // flags
        k_printf((section->sh_flags & SHF_STRINGS) ? "S" : "-");
        k_printf((section->sh_flags & SHF_EXECINSTR) ? "X" : "-");
        k_printf((section->sh_flags & SHF_ALLOC) ? "A" : "-");
        k_printf((section->sh_flags & SHF_WRITE) ? "W" : "-");
        k_printf("\t");
        // 
        k_printf("*%x\t%x\t%x\t", section->sh_addr, section->sh_size, section->sh_addralign);
        k_printf("%s", shstrtab + section->sh_name);

        k_printf("\n");
    }
}

Elf32_Shdr *elf_section_by_name(Elf32_Shdr *shdr, const char *sname);
