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

const char* symtypes[] = {
    [ STT_NOTYPE ] = "notype",
    [ STT_OBJECT ] = "object",
    [ STT_FUNC ] = "func",
    [ STT_SECTION ] = "section",
    [ STT_FILE ] = "file",
};
const char* symbinds[] = {
    [ STB_LOCAL ] = "L",
    [ STB_GLOBAL ] = "G",
    [ STB_WEAK ] = "W",
};


Elf32_Shdr *elf_section_by_name(Elf32_Shdr *shdr, size_t snum, const char *sname) {
    int i;
    Elf32_Shdr *section;
    const char *shstrtab = null;

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

    for (i = 0; i < snum; ++i) {
        section = shdr + i;
        if (!section->sh_name)
            continue;
        if (!strcmp(shstrtab + section->sh_name, sname))
            return section;
    }
    return null;
}

void print_elf_syms(Elf32_Sym *syms, size_t n_syms, const char *strtab, const char *symname) {
    int i;
    size_t symlen = strlen(symname);
    for (i = 0; i < n_syms; ++i) {
        Elf32_Sym *sym = syms + i;
        const char *name = strtab + sym->st_name;
        if (!strncmp(symname, name, symlen)) {
            k_printf("*%x\t[%x]\t", sym->st_value, sym->st_size);
            uint8_t type = ELF32_ST_TYPE(sym->st_info);
            k_printf("%s\t", (type <= STT_FILE) ? symtypes[type] : "???");
            type = ELF32_ST_BIND(sym->st_info);
            k_printf("%s\t", (type <= STB_WEAK) ? symbinds[type] : "?");
            k_printf("%s\n", name);
        }
    }
}

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
        k_printf("  *%x\t%x\t%x\t", section->sh_addr, section->sh_size, section->sh_addralign);
        k_printf("%s", shstrtab + section->sh_name);

        k_printf("\n");
    }
}

