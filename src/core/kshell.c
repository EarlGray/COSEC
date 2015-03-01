#include <arch/i386.h>
#include <arch/mboot.h>
#include <arch/multiboot.h>

#include <dev/intrs.h>
#include <dev/screen.h>
#include <dev/kbd.h>
#include <dev/pci.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include <mem/pmem.h>
#include <mem/kheap.h>
#include <misc/test.h>
#include <linux/elf.h>

#include <fs/vfs.h>
#include <log.h>

#include <kshell.h>
#include <ctype.h>

#ifdef COSEC_LUA
# include <lua/lua.h>
# include <lua/lualib.h>
# include <lua/lauxlib.h>
#endif

#define CMD_SIZE    256

/***
  *     Internal declarations
 ***/

bool kshell_autocomplete(char *buf);
static char * get_args(char *command);
static const char * get_int_opt(const char *arg, int *res, uint8_t base);
int run_lisp(void);


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
            clear_screen();
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
    //
}



/***
  *     Temporary kernel shell
  *    @TODO: must be moved to userspace as soon as possible
 ***/
typedef void (*void_f)();

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
#if COSEC_LUA
void kshell_lua();
#endif

void kshell_init() {
#ifdef TEST_USERSPACE
    test_init();
#else
    printf("test init not implemented\n");
#endif
}

const struct kshell_command main_commands[] = {
    {   .name = "init",     .handler = kshell_init,  .description = "userspace init()",
        .options = "just init!" },
    {   .name = "test",     .handler = kshell_test,  .description = "test utility",
        .options = "sprintf kbd timer serial tasks ring3 usleep" },
    {   .name = "info",     .handler = kshell_info,  .description = "various info",
        .options = "stack gdt pmem colors cpu pci irq mods mboot" },
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
#if COSEC_LUA
    {   .name = "lua",      .handler = kshell_lua,  .description = "Lua REPL", .options = "" },
#endif
    {   .name = NULL,       .handler = 0    }
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
    { .name = "str",     .handler = test_strs,      },
    { .name = 0, .handler = 0    },
};

void kshell_ls(const struct kshell_command __unused *this, const char *arg) {
    while (*arg && (*arg == ' ')) ++arg;
    print_ls(arg);
}

void kshell_vfs(const struct kshell_command __unused *this, const char *arg) {
    logmsgef("TODO: vfs");
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

/*
 *  parsing ELF
 */
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
    index_t i;
    Elf32_Shdr *section;
    const char *shstrtab = null;

    // find .shstrtab first
    for (i = 0; i < snum; ++i) {
        section = shdr + i;
        if (section->sh_type == SHT_STRTAB) {
            shstrtab = (void *)section->sh_addr;
            if (!strncmp(".shstrtab", shstrtab + section->sh_name, 9)) break;
            else shstrtab = null;
        }
    }

    assert(shstrtab, null, "shstrtab not found");

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
    index_t i;
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
    index_t i;

    // find .shstrtab first
    for (i = 0; i < snum; ++i) {
        section = shdr + i;
        if (section->sh_type == SHT_STRTAB) {
            shstrtab = (void *)section->sh_addr;
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
            set_cursor_attr(attr);
        else
            printf("color = %x\n", (uint)get_cursor_attr());
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

#ifdef COSEC_LUA
int syslua_heapinfo(lua_State *L) {
    kheap_info();
    return 0;
}

int syslua_mem(lua_State *L) {
    int argc = lua_gettop(L);
    uint p = 0;
    uint sz = 0x100;

    switch (argc) {
      case 2: sz = (uint)lua_tonumber(L, 2); /* fallthrough */
      case 1: p = (uint)lua_tonumber(L, 1); /* fallthrough */
      case 0: break;
    }

    print_mem((const char *)p, (size_t)sz);
    return 0;
}

void kshell_lua_test(void) {
    const char *prompt = "\x1b[36mlua> \x1b[3fm";
    char cmd_buf[CMD_SIZE];
    lua_State *lua;
    int luastack;

    logmsg("starting Lua...\n");
    k_printf("\x1b[32m###\n###       Lua "LUA_VERSION_MAJOR"."LUA_VERSION_MINOR"\n###\n");
    k_printf("###  type 'q' to exit\n###\x1b[0m\n");

    lua = luaL_newstate();
    if (!lua)
        logmsge("luaL_newstate() -> NULL");

    /* sys module */
    lua_newtable(lua);

    lua_pushstring(lua, "heapinfo");
    lua_pushcfunction(lua, syslua_heapinfo);
    lua_settable(lua, -3);

    lua_pushstring(lua, "mem");
    lua_pushcfunction(lua, syslua_mem);
    lua_settable(lua, -3);

    lua_setglobal(lua, "sys");


    luaL_openlibs(lua);

    for (;;) {
        kshell_readline(cmd_buf, CMD_SIZE, prompt);
        set_default_cursor_attr();
        if (!strcasecmp(cmd_buf, "q"))
            break;

        int err = luaL_loadbuffer(lua, cmd_buf, strlen(cmd_buf), "line")
               || lua_pcall(lua, 0, 0, 0);
        if (err) {
            fprintf(stderr, "### Error: %s\n", lua_tostring(lua, -1));
            lua_pop(lua, 1);
        }
    }

    lua_close(lua);
    logmsg("...back to kshell\n");
}

void kshell_lua() {
    int ret = exitpoint();
    if (ret == EXITENV_EXITPOINT) {
        kshell_lua_test();
    } else {
        logmsgif("... exit code %d", ret);
    }
}
#endif // COSEC_LUA

void kshell_unknown_cmd() {
    k_printf("type 'help'\n\n");
}

void kshell_panic() {
    panic("THIS IS JUST A TEST");
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

    k_printf("Available commands ('help <cmd>' for more):\n\t");
    for (cmd = main_commands; cmd->name; ++cmd)
        printf("%s\t", cmd->name);

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

    for (;;) {
        kshell_readline(cmd_buf, CMD_SIZE, prompt);
        kshell_do(cmd_buf);
    }
}
