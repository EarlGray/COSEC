#include <arch/i386.h>
#include <arch/mboot.h>
#include <arch/multiboot.h>

#include <dev/screen.h>
#include <dev/pci.h>

#include <std/string.h>
#include <std/stdio.h>
#include <std/time.h>

#include <mem/pmem.h>
#include <mem/kheap.h>
#include <misc/test.h>

#include <fs/vfs.h>
#include <fs/elf.h>
#include <log.h>

#include <kshell.h>

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
panic(const char *fatal_error) {
    intrs_disable();

    char buf [100] = { 0 };

    set_cursor_attr(0x4E);
    clear_screen();
    k_printf("\n");
    print_centered("O_o");
    print_centered("Oops, kernel panic");
    k_printf("\n");

    snprintf(buf, 100, "--> %s <--", fatal_error);
    print_centered(buf);
    /*
    k_printf("\nCPU: \n");
    print_cpu();
    k_printf("\n\nKernel stack: \n");
    print_mem((char *)cpu_stack(), 0x50);
    */

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

void console_readline(char *buf, size_t size) {
    char *cur = buf;
    while (1) {
        char c = getchar();

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
            clear_screen();
            console_write(prompt);
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
                putchar(c);
            }
        }
    }
}

static void console_setup(void) {
    //
}



/***
  *     Temporary kernel shell
  *    @TODO: must be moved to userspace as soon as possible
 ***/
typedef void (*void_f)(void);

struct kshell_command {
    const char * name;
    void (*handler)(const struct kshell_command *this, const char *arg);
    const char * description;
    const char * options;
};

struct kshell_subcmd {
    const char *name;
    void_f handler;
};


void kshell_help();
void kshell_info(const struct kshell_command *, const char *);
void kshell_mboot(const struct kshell_command *, const char *);
void kshell_test(const struct kshell_command *, const char *);
void kshell_heap(const struct kshell_command *, const char *);
void kshell_set(const struct kshell_command *, const char *);
void kshell_elf(const struct kshell_command *, const char *);
void kshell_mem(const struct kshell_command *, const char *);
void kshell_vfs(const struct kshell_command *, const char *);
void kshell_io(const struct kshell_command *, const char *);
void kshell_ls();
void kshell_mount();
void kshell_time();
void kshell_panic();

void kshell_init() {
    test_init();
}

const struct kshell_command main_commands[] = {
    {   .name = "init",     .handler = kshell_init,  .description = "userspace init()",
        .options = "just init!" },
    {   .name = "test",     .handler = kshell_test,  .description = "test utility",
        .options = "sprintf kbd timer serial tasks ring3 usr" },
    {   .name = "info",     .handler = kshell_info,  .description = "various info",
        .options = "stack gdt pmem colors cpu pci mods" },
    {   .name = "mem",      .handler = kshell_mem,   .description = "mem <start_addr> <size = 0x100>" },
    {   .name = "io",       .handler = kshell_io,    .description = "io[bwd][rw] <port> [<value>]",
        .options = "br/wr/dr <port> -- read port; bw/ww/dw <port> <value> - write value to port" },
    {   .name = "heap",     .handler = kshell_heap,  .description = "heap utility", .options = "info alloc free check" },
    {   .name = "vfs",      .handler = kshell_vfs,   .description = "vfs utility",  .options = "write read", },
    {   .name = "set",      .handler = kshell_set,   .description = "manage global variables", .options = "color prompt" },
    {   .name = "elf",      .handler = kshell_elf,   .description = "inspect ELF formats", .options = "sections syms" },
    {   .name = "panic",    .handler = kshell_panic, .description = "test The Red Screen of Death"     },
    {   .name = "help",     .handler = kshell_help,  .description = "show this help"   },
    {   .name = "ls",       .handler = kshell_ls,    .description = "list current VFS directory"    },
    {   .name = "mount",    .handler = kshell_mount, .description = "list current mounted filesystems"  },
    {   .name = "time",     .handler = kshell_time,  .description = "system time", .options = ""  },
    {   .name = null,       .handler = 0    }
};

const struct kshell_subcmd  test_cmds[] = {
    { .name = "sprintf", .handler = test_sprintf    },
    { .name = "timer",   .handler = test_timer,     },
    { .name = "serial",  .handler = test_serial,    },
    { .name = "kbd",     .handler = test_kbd,       },
    { .name = "tasks",   .handler = test_tasks,     },
    { .name = "ring3",   .handler = test_userspace, },
    { .name = 0, .handler = 0    },
};

void kshell_ls(const struct kshell_command *this, const char *arg) {
    while (*arg && (*arg == ' ')) ++arg;
    print_ls(arg);
}

void kshell_vfs(const struct kshell_command *this, const char *arg) {
    vfs_shell(arg);
}

void kshell_mount() {
    print_mount();
}

/* return number of first different symbol or endchar */
static inline size_t strcmpsz(const char *s1, const char *s2, char endchar) {
    size_t i;
    for (i = 0; (s1[i] == s2[i]) && (s1[i] != endchar); ++i);
    return i;
}

#define skip_gaps(pchar) \
    do while (' ' == *(pchar)) ++(pchar); while (0)

bool kshell_autocomplete(char *buf) {
    int i;
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
        char *sym = arg + 4;
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
            char attr = get_cursor_attr();
            k_printf("%x) ", i);
            set_cursor_attr(i); k_printf("aaaa ");
            set_cursor_attr((uint8_t)(i << 4)); k_printf("bbbb");
            set_cursor_attr(attr); k_printf("\n");
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
        get_int_opt(end, &slot, 16);
        if (*end == null) {
            printf("bus %x\n", bus, slot);
            pci_list(bus);
        } else {
            printf("bus %x, slot %x\n", bus, slot);
            pci_info(bus, slot);
        }
    } else
    if (!strncmp(arg, "mods", 4)) {
        count_t n_mods = 0;
        module_t *mods;
        mboot_modules_info(&n_mods, &mods);
        printf(" Modules loaded = %d\n", n_mods);
        int i;
        for_range(i, n_mods) {
            printf("%d '%s' - [ *%x : *%x ]\n",
                    i, (mods[i].string ? : "<null>"),
                    mods[i].mod_start, mods[i].mod_end);
        }
    } else {
        k_printf("Options: %s\n\n", this->options);
    }
}

void kshell_io(const struct kshell_command *this, const char *arg) {
    int port = 0;
    int val = 0;
    char *arg2 = get_int_opt(arg + 2, &port, 16);
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
            set_cursor_attr(attr);
        else
            printf("color = %x\n", (uint)get_cursor_attr());
    } else
    if (!strncmp(arg, "prompt", 6)) {
        if (*(arg + 6))
            strncpy(prompt, arg + 7, PROMPT_BUF_SIZE);
        else
            print("Why do you want to output prompt if you see it?!\n");
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
    for (subcmd = test_cmds; subcmd->name; ++subcmd)
        if (!strncmp(cmdline, subcmd->name, strlen(subcmd->name))) {
            (subcmd->handler)();
            return;
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

void kshell_unknown_cmd() {
    k_printf("type 'help'\n\n");
}

void kshell_panic() {
    //panic("THIS IS JUST A TEST");
    asm (".word 0xB9F0 \n");
}

void kshell_help(const struct kshell_command *this, const char *cmdline) {
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

    print("Available commands ('help <cmd>' for more):\n\t");
    for (cmd = main_commands; cmd->name; ++cmd)
        printf("%s\t", cmd->name);

    print("\n\nAvailable shortcuts:\n\tCtrl-L - clear screen\n\n");
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

#define for_ever while(1)
#define CMD_SIZE    256

void kshell_run(void) {
    char cmd_buf[CMD_SIZE] = { 0 };

    console_setup();

    for_ever {
        console_write(prompt);
        console_readline(cmd_buf, CMD_SIZE);
        kshell_do(cmd_buf);
    }
}
