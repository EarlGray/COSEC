#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#define __DEBUG
#include <cosec/log.h>

#include <arch/i386.h>
#include <arch/mboot.h>
#include <arch/multiboot.h>

#include <dev/intrs.h>
#include <dev/screen.h>
#include <dev/kbd.h>
#include <dev/tty.h>
#include <dev/pci.h>
#include <dev/acpi.h>

#include <mem/pmem.h>
#include <mem/kheap.h>
#include <misc/test.h>
#include <misc/elf.h>

#include <fs/vfs.h>
#include <fs/devices.h>
#include <process.h>

#include <kshell.h>
#include <ctype.h>

#define CMD_SIZE    256

/***
  *     Internal declarations
 ***/

bool kshell_autocomplete(char *buf);
static char * get_args(char *command);
static const char * get_int_opt(const char *arg, int *res, uint8_t base);

/***
  *     Panic and other print routines
 ***/

void __noreturn
kpanic(const char *fatal_error) {
    intrs_disable();

    char buf [100] = { 0 };

    vcsa_switch(CONSOLE_VCSA);
    vcsa_set_attribute(CONSOLE_VCSA, BACKGROUND(VCSA_ATTR_RED) | BRIGHT(VCSA_ATTR_ORANGE));
    vcsa_clear(CONSOLE_VCSA);

    k_printf("\n");
    print_centered("O_o");
    print_centered("Oops, kernel panic");
    k_printf("\n");

    snprintf(buf, 100, "--> %s <--", fatal_error);
    print_centered(buf);

    // original ascii-art from: http://brainbox.cc/repository/trunk/sixty-four/multiboot/include/kernel.h
    k_printf("\n"
"              ___  _____ \n"
"            .'/,-Y\"     \"~-. \n"
"            l.Y             ^. \n"
"            /\\               _\\_      \"Yaaaaah! oh-my-GOD!\n"
"           i            ___/\"   \"\\     We're trapped inside some\n"
"           |          /\"   \"\\   o !    computer-dimension-\n"
"           l         ]     o !__./     something-or-another!\"\n"
"            \\ _  _    \\.___./    \"~\\ \n"
"             X \\/ \\            ___./ \n"
"            ( \\ ___.   _..--~~\"   ~`-. \n"
"             ` Z,--   /               \\ \n"
"               \\__.  (   /       ______) \n"
"                 \\   l  /-----~~\" / \n"
"                  Y   \\          /\n"
"                  |    \"x______.^ \n"
"                  |           \\ \n"
"\n");

    cpu_hang();
}

void print_mem(const char *p, size_t count) {
    char buf[100] = { 0 };
    char printable[0x10 + 1] = { 0 };
    char s = 0;

    int printnum = 0;
    int rest = (uint)p % 0x10;
    if (rest) {
        int indent = 9 + 3 * rest + (rest >> 2) + (rest % 4? 1 : 0);
        while (indent-- > 0) {
            k_printf(" ");
        }
        while (rest--)
            printable[printnum++] = ' ';
    }

    size_t i;
    for (i = 0; i < count; ++i) {

        if (0 == (uint32_t)(p + i) % 0x10) {
            /* end of line */
            k_printf("%s| %s\n", buf, printable);
            printnum = 0;

            /* start next line */
            s = snprintf(buf, 100, "%0.8x:", (uint32_t)(p + i));
        }

        if (0 == (uint)(p + i) % 0x4) {
            s += snprintf(buf + s, 100 - s, " ");
        }

        int t = (uint8_t) p[i];
        s += snprintf(buf + s, 100 - s, "%0.2x ", t);

        if ((0x20 <= t) && (t < 0x80)) printable[printnum] = t;
        else if (0x80 <= t) printable[printnum] = '`';
        else printable[printnum] = '.';
        printnum++;
    }
    k_printf("%s| %s\n", buf, printable);
}

void print_intr_stack(const uint *const stack) {
#define LINE_SZ     80
    char line[LINE_SZ];
    snprintf(line, LINE_SZ, "\tEAX = %0.8x\t\tEDX = %0.8x\n", stack[7], stack[6]);
    k_printf("%s", line);
    snprintf(line, LINE_SZ, "\tECX = %0.8x\t\tEBX = %0.8x\n", stack[5], stack[4]);
    k_printf("%s", line);
    snprintf(line, LINE_SZ, "\tEBP = %0.8x\t\tESP = %0.8x\n", stack[3], stack[2]);
    k_printf("%s", line);
    snprintf(line, LINE_SZ, "\tESI = %0.8x\t\tEDI = %0.8x\n", stack[1], stack[0]);
    k_printf("%s", line);
}

void print_cpu(void) {
    char buf[100];
    i386_snapshot(buf);
    print_intr_stack((const uint *const)buf);
}


/***
  *     Console routines
 ***/

#define PROMPT_BUF_SIZE     30
char prompt[PROMPT_BUF_SIZE] = "|< ";

inline void console_write(const char *msg) {
    k_printf("%s", msg);
}

inline void console_writeline(const char *msg) {
    k_printf("%s\n", msg);
}

void kshell_readline(char *buf, size_t size, const char *prompt) {
    k_printf(prompt);
    char *cur = buf;
    while (1) {
        char c = kbd_getchar();

        switch (c) {
        case '\n':
            cur[0] = 0;
            k_printf("\n");
            return;
        case '\b':
            if (cur == buf) break;
            k_printf("\b \b");
            --cur;
            break;
        case 12:    // Ctrl-L
            *cur = 0;
            vcsa_clear(CONSOLE_VCSA);
            k_printf(prompt);
            console_write(buf);
            break;
        case '\t':
            cur[1] = 0;
            if (kshell_autocomplete(buf)) return;
            break;
        default:
            if (cur - buf + 1 < (int)size) {
                *cur = c;
                ++cur;
                cprint(c);
            }
        }
    }
}

static void console_setup(void) {
    k_printf("\n");
}



/***
  *     Temporary kernel shell
  *    @TODO: must be moved to userspace as soon as possible
 ***/
typedef void (*void_f)();

struct kshell_subcmd {
    const char *name;
    void_f handler;
};

struct kshell_command {
    const char * name;
    void (*handler)(const struct kshell_command *this, const char *arg);
    const char * description;
    const char * options;
    struct kshell_subcmd *subcmds;
};


void kshell_help();
void kshell_info(const struct kshell_command *, const char *);
void kshell_cpuid(const struct kshell_command *, const char *);
void kshell_mboot(const struct kshell_command *, const char *);
void kshell_test(const struct kshell_command *, const char *);
void kshell_heap(const struct kshell_command *, const char *);
void kshell_set(const struct kshell_command *, const char *);
void kshell_elf(const struct kshell_command *, const char *);
void kshell_mem(const struct kshell_command *, const char *);
void kshell_vfs(const struct kshell_command *, const char *);
void kshell_io(const struct kshell_command *, const char *);
void kshell_ls();
void kshell_time();
void kshell_panic();
void kshell_off();
#if COSEC_LUA
void kshell_lua();
#endif

void kshell_init() {
    run_init();
}

const struct kshell_command main_commands[] = {
    { .name = "init",
        .handler = kshell_init,
        .description = "userspace init()",
        .options = "just init!" },
    { .name = "test",
        .handler = kshell_test,
        .description = "test utility",
        .options = "sprintf kbd timer serial tasks acpi ring3 usleep" },
    { .name = "info",
        .handler = kshell_info,
        .description = "various info",
        .options = "stack gdt pmem colors cpu pci irq mods mboot" },
    { .name = "cpuid",
        .handler = kshell_cpuid,
        .description = "x86 cpuid info; usage: cpuid [function, default 0]",
        .options = "cpuid [<funct>=0]", },
    { .name = "mem",
        .handler = kshell_mem,
        .description = "mem <start_addr> <size = 0x100>" },
    { .name = "io",
        .handler = kshell_io,
        .description = "io[bwd][rw] <port> [<value>]",
        .options = "\n\tbr/wr/ir <port> -- read port;"
                   "\n\tbw/ww/iw <port> <value> - write value to port" },
    { .name = "heap",
        .handler = kshell_heap,
        .description = "heap utility",
        .options = "info alloc free check" },
    { .name = "fs",
        .handler = kshell_vfs,
        .description = "vfs utility",
        .options =
            "\n  mounted                 -- list mountpoints;"
            "\n  ls /absolute/dir/path   -- print directory entries list;"
            "\n  stat /abs/path/to/flie  -- print `struct stat *` info;"
            "\n  mkdir /abs/path/to/dir  -- create a directory;"
            "\n  mknod /abs/path/to/file -- create a regular file;"
            "\n  mkdev [c|d] <maj>:<min> /abs/path/to/dev -- create a device file"
            "\n  ln /existing/path /new  -- create a new hard link"
            "\n  mv /abs/path /new/path  -- rename file"
            "\n  rm /abs/path            -- unlink path (possibly its inode)"
            "\n  cat [>] /abs/path       -- read/write file"
        },
    { .name = "set",
        .handler = kshell_set,
        .description = "manage global variables",
        .options = "color prompt" },
    { .name = "elf",
        .handler = kshell_elf,
        .description = "inspect ELF formats",
        .options = "sections syms" },
    { .name = "panic",
        .handler = kshell_panic,
        .description = "test The Red Screen of Death"     },
    { .name = "help",
        .handler = kshell_help,
        .description = "show this help"   },
    { .name = "time",
        .handler = kshell_time,
        .description = "system time",
        .options = ""  },
    { .name = "off",
        .handler = kshell_off,
        .description = "ACPI poweroff",
        .options = "" },
#if COSEC_LUA
    { .name = "lua",
        .handler = kshell_lua,
        .description = "Lua REPL",
        .options = "" },
#endif
    { .name = NULL,
        .handler = 0    }
};

void test_strs(void);
const struct kshell_subcmd  test_cmds[] = {
    { .name = "sprintf", .handler = test_sprintf    },
    { .name = "timer",   .handler = test_timer,     },
    { .name = "serial",  .handler = test_serial,    },
    { .name = "kbd",     .handler = test_kbd,       },
    { .name = "tasks",   .handler = test_tasks,     },
    { .name = "ring3",   .handler = test_userspace, },
    { .name = "usleep",  .handler = test_usleep,    },
    { .name = "acpi",    .handler = test_acpi,      },
    { .name = "str",     .handler = test_strs,      },
    { .name = 0, .handler = 0    },
};



/* return number of first different symbol or endchar */
static inline size_t strcmpsz(const char *s1, const char *s2, char endchar) {
    size_t i;
    for (i = 0; (s1[i] == s2[i]) && (s1[i] != endchar); ++i);
    return i;
}

#define skip_gaps(pchar) \
    do while (' ' == *(pchar)) ++(pchar); while (0)

bool kshell_autocomplete(char *buf) {
    size_t i;
    const struct kshell_command *cmd;

    // search for commands
    for (cmd = main_commands; cmd->name; ++cmd) {
        i = strcmpsz(cmd->name, buf, 0);
        if (i == strlen(cmd->name)) {
            // full main command
            char *rest = buf + i;
            skip_gaps(rest);
            if (0 == *rest) {
                k_printf("\n%s\n%s%s", cmd->options, prompt, buf);
                return false;
            }
            // else we need to track options of cmd
            const char *opt = cmd->options;
            i = strcmpsz(opt, rest, ' ');
            return false;
        }
        if (i == strlen(buf)) {
            // complete main command
            strcpy(buf + i, cmd->name + i);
            k_printf("%s ", cmd->name + i);
            return true;
        }
    }
    return false;
}

void kshell_elf(const struct kshell_command *this, const char *arg) {
    elf_section_header_table_t *mboot_syms = mboot_kernel_shdr();
    assertv(mboot_syms, "mboot.syms are NULL\n");
    Elf32_Shdr *shdrs = (Elf32_Shdr *)mboot_syms->addr;
    size_t n_shdr = mboot_syms->num;

    if (!strncmp(arg, "sections", 8)) {
        print_section_headers(shdrs, n_shdr);
    } else if (!strncmp(arg, "syms", 4)) {
        const char *sym = arg + 4;
        skip_gaps(sym);
        Elf32_Shdr *symtab = elf_section_by_name(shdrs, n_shdr, ".symtab");
        Elf32_Shdr *strsect = elf_section_by_name(shdrs, n_shdr, ".strtab");
        char *strtab = (char *) strsect->sh_addr;
        print_elf_syms((Elf32_Sym *)symtab->sh_addr, symtab->sh_size / sizeof(Elf32_Sym),
                strtab, sym);
    } else {
        k_printf("Options: %s\n", this->options);
    }
}

void kshell_cpuid(const struct kshell_command *this, const char *arg) {
    UNUSED(this);
    uint funct = 0;
    uint ret;
    union {
        char as_str[13];
        struct { uint ebx, ecx, edx; } as_reg;
    } cpu_info;
    cpu_info.as_str[12] = 0;

    ret = i386_cpuid_check();
    if (!ret) { k_printf("CPUID is not supported, weird\n"); return; }

    while (isspace(arg[0])) ++arg;
    funct = atoi(arg);  /* 0 is ok */

    ret = i386_cpuid_info(&cpu_info, funct);
    if (funct) {
        k_printf("funct = 0x%x\n", funct);
        k_printf("  %%eax = 0x%x\n  %%ebx = 0x%x\n  %%ecx = 0x%x\n  %%edx = %x\n",
                    ret, cpu_info.as_reg.ebx, cpu_info.as_reg.ecx, cpu_info.as_reg.edx);
    } else {
        k_printf("%%eax = %x, CPU vendor: %s\n", ret, cpu_info.as_str);
    }
}

void kshell_heap(const struct kshell_command *this, const char *arg) {
    if (!strncmp(arg, "info", 4))
        kheap_info();
    else
    if (!strncmp(arg, "alloc", 5)) {
        size_t size = 0;
        arg += 5;
        const char *end = get_int_opt(arg, (int *)&size, 16);
        if (end == arg) return;

        void *p = kmalloc(size);
        k_printf("kmalloc(%x) = *%x\n", (uint)size, (uint)p);
    } else
    if (!strncmp(arg, "free", 4)) {
        ptr_t p = 0;
        arg += 4;
        const char *end = get_int_opt(arg, (int *)&p, 16);
        if (end == arg) return ;

        kfree((void *)p);
        k_printf("kfree(*%x)\n", (uint)p);
    } else
    if (!strncmp(arg, "check", 5)) {
        k_printf("heap check = *0x%x\n", kheap_check());
    } else {
        k_printf("Options: %s\n\n", this->options);
    }
}

void kshell_info(const struct kshell_command *this, const char *arg) {
    if (!strcmp(arg, "stack")) {
        k_printf("stack at 0x%x\n", (uint)cpu_stack());
    } else
    if (!strcmp(arg, "gdt")) {
        k_printf("GDT is at 0x%x\n", (uint)i386_gdt());
    } else
    if (!strcmp(arg, "pmem")) {
        pmem_info();
    } else
    if (!strcmp(arg, "colors")) {
        k_printf("Colors:\n");
        int i = 0;
        for (i = 0; i < 16; ++i) {
            char attr = vcsa_get_attribute(CONSOLE_VCSA);

            k_printf("%x) ", i);
            vcsa_set_attribute(CONSOLE_VCSA, i);                 k_printf("aaaa ");
            vcsa_set_attribute(CONSOLE_VCSA, (uint8_t)(i << 4)); k_printf("bbbb");
            vcsa_set_attribute(CONSOLE_VCSA, attr);              k_printf("\n");
        }
        k_printf("\n");
    } else
    if (!strcmp(arg, "cpu")) {
        print_cpu();
    } else
    if (!strncmp(arg, "pci", 3)) {
        int bus = 0, slot = -1;
        arg += 3;
        const char *end = get_int_opt(arg, &bus, 16);
        if (*end == '\0') {
            printf("bus %x\n", bus);
            pci_list(bus);
        } else {
            get_int_opt(end, &slot, 16);
            printf("bus %x, slot %x\n", bus, slot);
            pci_info(bus, slot);
        }
    } else
    if (!strncmp(arg, "irq", 3)) {
        k_printf("IRQ mask: %x\n", (uint)irq_get_mask() & 0xffff);
    } else
    if (!strncmp(arg, "mboot", 5)) {
        print_mboot_info();
    } else
    if (!strncmp(arg, "mods", 4)) {
        count_t n_mods = 0;
        module_t *mods;
        mboot_modules_info(&n_mods, &mods);
        printf(" Modules loaded = %d\n", n_mods);
        size_t i;
        for(i = 0; i < n_mods; ++i) {
            printf("%d '%s' - [ *%x : *%x ]\n",
                    i, (mods[i].string ? (const char *)mods[i].string : "<null>"),
                    mods[i].mod_start, mods[i].mod_end);
        }
    } else {
        k_printf("Options: %s\n\n", this->options);
    }
}

void kshell_io(const struct kshell_command *this, const char *arg) {
    int port = 0;
    int val = 0;
    const char *arg2 = get_int_opt(arg + 2, &port, 16);
    switch (arg[1]) {
    case 'r':
        switch (arg[0]) {
          case 'b': inb(port, val); break;
          case 'w': inw(port, val); break;
          case 'i': inl(port, val); break;
          default: goto err;
        }
        printf("in(0x%x) => 0x%x\n", port, val);
        break;
    case 'w': {
        get_int_opt(arg2, &val, 16);
        printf("out(0x%x) => 0x%x\n", port, val);
        switch (arg[0]) {
          case 'b': outb(port, val); break;
          case 'w': outw(port, val); break;
          case 'i': outl(port, val); break;
          default: goto err;
        }
        } break;
    default: goto err;
    }
    return;
err:
    printf("Options:\n%s\n\n", this->options);
}

void kshell_set(const struct kshell_command *this, const char *arg) {
    if (!strncmp(arg, "color", 5)) {
        arg += 5;
        int attr;
        const char *end = get_int_opt(arg, &attr, 16);
        if (end != arg)
            vcsa_set_attribute(CONSOLE_VCSA, attr);
        else {
            printf("color = %x\n", (uint)vcsa_get_attribute(VIDEOMEM_VCSA));
        }
    } else
    if (!strncmp(arg, "prompt", 6)) {
        if (*(arg + 6))
            strncpy(prompt, arg + 7, PROMPT_BUF_SIZE);
        else
            k_printf("Why do you want to output prompt if you see it?!\n");
    } else {
        k_printf("Variables: %s\n", this->options);
    }
}

void kshell_mem(const struct kshell_command *this, const char *arg) {
    uint addr, size;
    arg = get_int_opt(arg, (int *)&addr, 16);
    if (addr == 0) {
        k_printf("%s warning: reading 0x0000, default\n", this->name);
    }

    arg = get_int_opt(arg, (int *)&size, 16);
    if (size == 0)
        size = 0x100;

    print_mem((void *)addr, size);
    k_printf("\n");
}

void kshell_test(const struct kshell_command *this, const char *cmdline) {
    const struct kshell_subcmd *subcmd;
    for (subcmd = test_cmds; subcmd->name; ++subcmd) {
        size_t subcmdlen = strlen(subcmd->name);
        if (!strncmp(cmdline, subcmd->name, subcmdlen)) {
            cmdline = cmdline + subcmdlen;
            while (isspace(*cmdline)) ++cmdline;
            (subcmd->handler)(cmdline);
            return;
        }
    }

    k_printf("Options: %s\n\n", this->options);
}

void kshell_time() {
    ymd_hms t;
    time_ymd_from_rtc(&t);

    printf("Date: %d/%d/%d, %d:%d:%d\n",
            (uint)t.tm_mday, (uint)t.tm_mon, (uint)t.tm_year,
            (uint)t.tm_hour, (uint)t.tm_min, (uint)t.tm_sec);
}


static void fs_cat(const char *arg) {
    if (!arg[0]) { k_printf("Error: filepath or '>' expected\n"); return; }

    int ret;
    char lnbuf[256];
    size_t pos = 0;
    size_t nwritten;

    mountnode *sb = NULL;
    inode_t ino;

    if (arg[0] == '>') {
        ++arg; while (isspace(*arg)) ++arg;

        ret = vfs_lookup(arg, &sb, &ino);
        if (ret) { k_printf("vfs_lookup failed: %s\n", strerror(ret)); return; }

        k_printf("### EOF<Enter> will terminate input\n");
        while (true) {
            ret = tty_read(CONSOLE_TTY, lnbuf, 256, &nwritten);
            if (ret) { k_printf("tty_read failed: %s\n", strerror(ret)); return; }

            lnbuf[nwritten] = 0;
            lnbuf[nwritten - 1] = 0;
            logmsgf("fs_cat: line[%d]: '%s'\n", nwritten, lnbuf);
            lnbuf[nwritten - 1] = '\n';

            if (!strncmp(lnbuf, "EOF", 3)) return;

            ret = vfs_inode_write(sb, ino, pos, lnbuf, strlen(lnbuf), &nwritten);
            if (ret) { k_printf("inode_write failed: %s\n", strerror(ret)); return; }

            logmsgf("vfs_inode_write: nwritten=%d\n", nwritten);
            pos += nwritten;
        }
    } else {
        ret = vfs_lookup(arg, &sb, &ino);
        if (ret) { k_printf("vfs_lookup('%s') failed: %s\n", arg, strerror(ret)); return; }

        do {
            if (ret) { k_printf("vfs_lookup failed: %s\n", strerror(ret)); return; }

            int ret = vfs_inode_read(sb, ino, pos, lnbuf, 256, &nwritten);
            if (ret) { k_printf("Error: inode_read: %s\n", strerror(ret)); return; }

            tty_write(CONSOLE_TTY, lnbuf, nwritten);
            pos += nwritten;
        } while (nwritten);
    }
}

void kshell_vfs(const struct kshell_command __unused *this, const char *arg) {
    if (!strncmp(arg, "ls", 2)) {
        arg += 2; while (isspace(*arg)) ++arg;
        print_ls(arg);
    } else
    if (!strncmp(arg, "mounted", 7)) {
        print_mount();
    } else
    if (!strncmp(arg, "mkdir", 5)) {
        arg += 5; while (isspace(*arg)) ++arg;

        if (arg[0] == 0) { k_printf("mkdir: needs name\n"); return; }

        int ret = vfs_mkdir(arg, 0755);
        if (ret) k_printf("mkdir failed: %s\n", strerror(ret));
    } else
    if (!strncmp(arg, "mknod", 5)) {
        arg += 5; while (isspace(*arg)) ++arg;

        int ret = vfs_mknod(arg, 0644, 0);
        if (ret) k_printf("mknod failed: %s\n", strerror(ret));
    } else
    if (!strncmp(arg, "mkdev", 5)) {
        arg += 5; while (isspace(*arg)) ++arg;
        mode_t mode = 0755;
        switch (arg[0]) {
            case 'c': mode |= S_IFCHR; break;
            case 'b': mode |= S_IFBLK; break;
            default: k_printf("A device type expected: 'c' (chardev) or 'b' (blockdev)\n"); return;
        }
        while (isspace(*++arg));
        char *eptr;
        long maj = strtol(arg, &eptr, 0);
        returnv_err_if(eptr == arg, "A device major number expected");
        arg = eptr;
        returnv_err_if(arg[0] != ':', "A semicolon expected");
        long min = strtol(++arg, &eptr, 0);
        returnv_err_if(eptr == arg, "A device minor number expected");
        arg = eptr;
        while (isspace(*arg)) ++arg;

        int ret = vfs_mknod(arg, mode, gnu_dev_makedev(maj, min));
        if (ret) { k_printf("vfs_mknod() failed: %s\n", strerror(ret)); }
    } else
    if (!strncmp(arg, "stat", 4)) {
        arg += 4; while (isspace(*arg)) ++arg;

        char mode[11];

        struct stat stat;
        int ret = vfs_stat(arg, &stat);
        if (ret) { k_printf("stat failed: %s\n", strerror(ret)); return ; }

        k_printf("  st_dev    = %d:%d\n", gnu_dev_major(stat.st_dev), gnu_dev_minor(stat.st_dev));
        k_printf("  st_ino    = %d\n", stat.st_ino);

        strmode(stat.st_mode, mode);
        k_printf("  st_mode   = %s", mode);
        switch (mode[0]) {
            case 'c': case 'b':
                k_printf("; %d:%d", gnu_dev_major(stat.st_rdev), gnu_dev_minor(stat.st_rdev));
                break;
        }
        k_printf("\n  st_nlink  = %d\n", stat.st_nlink);
        k_printf("  st_size   = %d\n", stat.st_size);
    } else
    if (!strncmp(arg, "mv", 2)) {
        arg += 2; while (isspace(*arg)) ++arg;

        char *path2 = strchr(arg, ' ');
        if (!path2) { k_printf("Error: the new file name not found\n"); return; }

        char *path1 = strndup(arg, path2 - arg);

        while (isspace(*path2)) ++path2;

        int ret = vfs_rename(path1, path2);
        if (ret) k_printf("Error: %s\n", strerror(ret));

        kfree(path1);
    } else
    if (!strncmp(arg, "rm", 2)) {
        arg += 2; while (isspace(*arg)) ++arg;
        int ret = vfs_unlink(arg);
        if (ret) k_printf("rm failed: %s\n", strerror(ret));
    } else
    if (!strncmp(arg, "ln", 2)) {
        arg += 2; while (isspace(*arg)) ++arg;

        char *path2 = strchr(arg, ' ');
        if (!path2) { k_printf("Error: the new file name not found\n"); return; }

        char *path1 = strndup(arg, path2 - arg);

        while (isspace(*path2)) ++path2;

        int ret = vfs_hardlink(path1, path2);
        if (ret) k_printf("Error: %s\n", strerror(ret));

        kfree(path1);
    } else if (!strncmp(arg, "cat", 3)) {
        arg += 3; while (isspace(*arg)) ++arg;

        fs_cat(arg);
    } else {
        k_printf("Options: %s\n", this->options);
    }
}

void kshell_off() {
    acpi_poweroff();
}

void kshell_unknown_cmd() {
    k_printf("type 'help'\n\n");
}

void kshell_panic() {
    kpanic("THIS IS JUST A TEST");
    //asm (".word 0xB9F0 \n");
}

void kshell_help(const struct kshell_command __unused *this, const char *cmdline) {
    const struct kshell_command *cmd;
    if (*cmdline) {
        for (cmd = main_commands; cmd->name; ++cmd)
            if (!strcmp(cmd->name, cmdline)) {
                printf("\n\t%s\n", cmd->name);
                printf("Description: %s\n", cmd->description);
                printf("Options:     %s\n", cmd->options);
                return;
            }
    }

    k_printf("Available commands ('help <cmd>' for more):");
    int i = 0;
    for (cmd = main_commands; cmd->name; ++cmd) {
        if (i++ % 8 == 0) k_printf("\n\t");
        printf("%s\t", cmd->name);
    }

    k_printf("\n\nAvailable shortcuts:\n\tCtrl-L - clear screen\n\n");
}


static char * get_args(char *command) {
    // find the first gap
    char *arg;
    for (arg = command; *arg; ++arg)
        if (*arg == ' ') {
            while (*arg == ' ')
                *(arg++) = 0;
            break;
        }
    return arg;
}

static const char * get_int_opt(const char *arg, int *res, uint8_t base) {
    const char *end = arg;
    do {
        end = sscan_int(arg, res, base);
        if (end != arg) return end;
    } while (*(arg++));
    return end;
}

void kshell_do(char *command) {
    // empty command
    if (0 == *command)
        return;

    char *arg = get_args(command);

    const struct kshell_command *cmd;
    for (cmd = main_commands; cmd->name; ++cmd)
        if (!strcmp(cmd->name, command)) {
            cmd->handler(cmd, arg);
            return;
        }

    kshell_unknown_cmd(command);
}

void kshell_run(void) {
    char cmd_buf[CMD_SIZE] = { 0 };

    console_setup();

#if 1
    for (;;) {
        int ret;
        size_t nread;
        ret = tty_write(CONSOLE_TTY, prompt, strlen(prompt));
        if (ret) { k_printf("ERROR: tty_write: %s\n", strerror(ret)); cpu_hang(); }

        kbd_set_onpress(tty_keyboard_handler);
        kbd_set_onrelease(tty_keyboard_handler);

        ret = tty_read(CONSOLE_TTY, cmd_buf, CMD_SIZE, &nread);
        if (ret) k_printf("ERROR: tty_read(nread=%d): %s\n", nread, strerror(ret));

        if (cmd_buf[nread - 1] == '\n') cmd_buf[nread - 1] = 0;
        else logmsgf("kshell: cmd_buf[last] != \\n\n");

        logmsgf("kshell: nread=%d, cmd='%s'\n", nread, cmd_buf);

        kshell_do(cmd_buf);
    }
#else
    for (;;) {
        kshell_readline(cmd_buf, CMD_SIZE, prompt);
        kshell_do(cmd_buf);
    }
#endif
}

