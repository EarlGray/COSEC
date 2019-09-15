#include <stdlib.h>
#include <string.h>

#include <cosec/log.h>

#include <arch/multiboot.h>
#include <arch/mboot.h>

#define MAX_CMDLINE_LEN   256

struct vbe_info {
    void *control_info;
    void *mode_info;
    void *mode;
    void *interface_seg;
    void *interface_off;
    void *interface_len;
};

struct framebuffer {
    void *fb_addr;
    uint32_t fb_addr0;
    uint32_t fb_pitch;
    uint32_t fb_width;
    uint32_t fb_height;
    uint32_t fb_bpp;
    uint8_t fb_type;
    uint8_t fb_colors[5];
};

struct ext_info {
    union {
        uint32_t word;
        struct {
            uint8_t mem:1;
            uint8_t bootdev:1;
            uint8_t cmdline:1;
            uint8_t mods:1;
            uint8_t syms:2;
            uint8_t mmap:1;
            uint8_t drives:1;
            uint8_t configtable:1;
            uint8_t bootloader:1;
            uint8_t apmtable:1;
            uint8_t vbe_info:1;
            uint8_t framebuf_info:1;
        } bit;
    } flags;
    uint mem_lower;
    uint mem_upper;
    uint boot_device;
    size_t mods_count;
    module_t* mods_addr;
    elf_section_header_table_t* syms;
    uint mmap_length;
    void * mmap_addr;
    uint drives_length;
    void * drives_addr;
    uint config_table;
    const char *bootloader_name;
    void *apm_table;
    struct vbe_info *vbe;
    struct framebuffer *framebuf;
    char cmdline[MAX_CMDLINE_LEN];
} mboot;

void mboot_info_parse(struct multiboot_info *mbi) {
    k_printf("multiboot_info at *%x, flags 0x%x\n", (uint)mbi, mbi->flags);
    mboot.flags.word = mbi->flags;
    if (mboot.flags.bit.mem) {
        mboot.mem_lower = mbi->mem_lower;
        mboot.mem_upper = mbi->mem_upper;
        k_printf("mem_lower = %d Kb, mem_upper = %d Kb\n", mboot.mem_lower, mboot.mem_upper);
    } else {
        mboot.mem_lower = 0;
        mboot.mem_upper = 0;
    }
    mboot.boot_device = mboot.flags.bit.bootdev ? mbi->boot_device : 0;
    if (mboot.flags.bit.cmdline) {
        strncpy(mboot.cmdline, (const char *)mbi->cmdline, MAX_CMDLINE_LEN);
    } else {
        mboot.cmdline[0] = '\0';
    }
    if (mboot.flags.bit.mods) {
        mboot.mods_count = mbi->mods_count;
        mboot.mods_addr = (module_t *)mbi->mods_addr;
    } else {
        mboot.mods_count = 0;
        mboot.mods_addr = 0;
    }
    mboot.syms = (elf_section_header_table_t *)(mboot.flags.bit.syms ? &(mbi->u) : NULL);
    if (mboot.flags.bit.mmap) {
        mboot.mmap_length = mbi->mmap_length;
        mboot.mmap_addr = (void *)(mbi->mmap_addr);
    } else {
        mboot.mmap_length = 0;
        mboot.mmap_addr = NULL;
    }

    uint *mbia = (uint *)mbi;

    if (mboot.flags.bit.drives) {
        mboot.drives_length = mbia[13];
        mboot.drives_addr = (void *)mbia[14];
    } else {
        mboot.drives_length = 0;
        mboot.drives_addr = NULL;
    }
    mboot.config_table = (mboot.flags.bit.configtable ? (uint)(mbia[15]) : 0);
    mboot.bootloader_name = (mboot.flags.bit.bootloader ? (const char *)(mbia[16]) : NULL );
    mboot.apm_table = (mboot.flags.bit.apmtable ? (void *)(mbia[17]) : NULL);
    mboot.vbe = (mboot.flags.bit.vbe_info ? (void *)(mbia + 18) : NULL);
}

const char* mboot_cmdline(void)      { return mboot.cmdline;       }
uint32_t    mboot_mmap_length(void)  { return mboot.mmap_length / sizeof(memory_map_t);   }
void *      mboot_mmap_addr(void)    { return mboot.mmap_addr;     }
uint32_t    moot_drives_length(void) { return mboot.drives_length; }

elf_section_header_table_t* mboot_kernel_shdr(void) {
    return mboot.syms;
}

void mboot_modules_info(count_t *count, module_t **modules) {
    *count = mboot.mods_count;
    *modules = mboot.mods_addr;
}

uint mboot_uppermem_kb(void) {
    return mboot.mem_upper;
}

void print_mboot_info(void) {
    if (mboot.bootloader_name)      k_printf("Booted with: %s\n", mboot.bootloader_name);
    if (mboot.boot_device)          k_printf("Boot device = 0x%x\n", mboot.boot_device);
    if (mboot.mem_lower)            k_printf("Memory: lower=%d Kb, higher=%d Kb\n", mboot.mem_lower, mboot.mem_upper);
    if (mboot.mmap_length)          k_printf("mmap info: *0x%x [%d]\n", mboot.mmap_addr, mboot.mmap_length);
    if (mboot.drives_length)        k_printf("Drives count: %d\n", mboot.drives_length);
    if (mboot.cmdline[0])           k_printf("Kernel cmdline: %s\n", mboot.cmdline);
    if (mboot.syms)                 k_printf("Kernel symbols: *%x\n", mboot.syms);
    if (mboot.mods_count) {
        k_printf("Modules count: %d\n", mboot.mods_count);
        module_t *mod = (module_t *)mboot.mods_addr;
        for (size_t i = 0; i < mboot.mods_count; ++i)
            k_printf("  [%d] *%08x - *%08x : %s\n", i, mod[i].mod_start, mod[i].mod_end, (char *)mod[i].string);
    }
    if (mboot.config_table)         k_printf("config_table: *%x\n", mboot.config_table);
    if (mboot.vbe)                  k_printf("vbe info: *%x\n", mboot.vbe);
    if (mboot.framebuf)             k_printf("framebuf info: *%x\n", mboot.framebuf);
}
