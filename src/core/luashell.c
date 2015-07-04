#include <string.h>

#include <dev/screen.h>
#include <arch/i386.h>
#include <arch/multiboot.h>
#include <arch/mboot.h>
#include <misc/elf.h>
#include <kshell.h>

#include <cosec/log.h>

#ifdef COSEC_LUA

#include <lua/lua.h>
#include <lua/lualib.h>
#include <lua/lauxlib.h>

#define CMD_SIZE 256

#define LUA_ERROR(lua, msg) do {      \
    lua_pushstring((lua), (msg));     \
    lua_error(lua);                   \
} while (0)

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

int syslua_inb(lua_State *L) {
    const char *funcname = __FUNCTION__;
    int argc = lua_gettop(L);
    if (argc < 1)
        LUA_ERROR(L, "expected a port to read from");
    if (!lua_isnumber(L, 1))
        LUA_ERROR(L, "port number is not a number");

    uint16_t port = (uint16_t)lua_tonumber(L, 1);
    uint8_t val = 0;
    inb(port, val);
    lua_pushnumber(L, val);
    logmsgdf("%s(%d) -> %d\n", funcname, (int)port, (int)val);
    return 1; /* 1 result */
}

int syslua_outb(lua_State *L) {
    const char *funcname = __FUNCTION__;
    int argc = lua_gettop(L);
    if (argc != 2)
        LUA_ERROR(L, "arguments must be <port> and <val>");
    if (!lua_isnumber(L, 1) || !lua_isnumber(L, 2))
        LUA_ERROR(L, "arguments must be numbers");

    uint16_t port = (uint16_t)lua_tonumber(L, 1);
    uint8_t  val  = (uint8_t)lua_tonumber(L, 2);
    outb(port, val);
    logmsgdf("%s(%d, %d)\n", funcname, (int)port, (int)val);
    return 0;
}

int syslua_symaddr(lua_State *L) {
    const char *funcname = __FUNCTION__;
    int argc = lua_gettop(L);
    if (argc != 1)
        LUA_ERROR(L, "arguments must be <symbol>");
    if (!lua_isstring(L, 1))
        LUA_ERROR(L, "<symbol> must be string");

    const char *symname = lua_tostring(L, 1);
    logmsgf("%s(%s)\n", funcname, symname);

    elf_section_header_table_t *mboot_syms = mboot_kernel_shdr();
    if (NULL == mboot_syms)
        LUA_ERROR(L, "mboot.syms are NULL");

    Elf32_Shdr *shdrs = (Elf32_Shdr *)mboot_syms->addr;

    Elf32_Shdr *strsect = elf_section_by_name(shdrs, mboot_syms->num, ".strtab");
    char *strtab = (char *) strsect->sh_addr;

    Elf32_Shdr *symsect = elf_section_by_name(shdrs, mboot_syms->num, ".symtab");
    Elf32_Sym *syms = (Elf32_Sym *)symsect->sh_addr;

    index_t i;
    size_t symlen = strlen(symname);
    for (i = 0; i < symsect->sh_size/sizeof(Elf32_Sym); ++i) {
        Elf32_Sym *sym = syms + i;
        const char *name = strtab + sym->st_name;
        if (!strncmp(symname, name, symlen)) {
            lua_pushnumber(L, sym->st_value);
            return 1;
        }
    }

    lua_pushnil(L);
    return 1;
}

int lua_toint(lua_State *L, int index) {
    if (lua_isnumber(L, index))
        return (int)lua_tonumber(L, index);
    if (lua_isstring(L, index))
        return (int)lua_tostring(L, index);
    if (lua_isboolean(L, index))
        return (int)lua_toboolean(L, index);
    if (lua_isnil(L, index))
        return 0;
    LUA_ERROR(L, "<argN> is not converible to native type");
    return LUA_ERRERR;
}

int syslua_ccall(lua_State *L) {
    int argc = lua_gettop(L);
    int arg1 = 0, arg2 = 0, arg3 = 0;
    switch (argc) {
      case 4: arg3 = lua_toint(L, 4); /* fallthrough */
      case 3: arg2 = lua_toint(L, 3); /* fallthrough */
      case 2: arg1 = lua_toint(L, 2); /* fallthrough */
      case 1: break;
      default: LUA_ERROR(L, "more than 3 arguments are not supported");
    }

    void *callee = (void *)(ptr_t)lua_tonumber(L, 1);
    int ret;
    switch (argc) {
      case 1: ret = ((int (*)(void))callee)(); break;
      case 2: ret = ((int (*)(int))callee)(arg1); break;
      case 3: ret = ((int (*)(int, int))callee)(arg1, arg2); break;
      case 4: ret = ((int (*)(int, int, int))callee)(arg1, arg2, arg3); break;
      default: return 0;
    }

    lua_pushnumber(L, ret);
    return 1;
}

struct luamod_entry {
    const char *fname;
    int (*fptr)(lua_State *);
};

const struct luamod_entry luamod_sys[] = {
    { .fname = "mem",       .fptr = syslua_mem      },
    { .fname = "inb",       .fptr = syslua_inb      },
    { .fname = "outb",      .fptr = syslua_outb     },
    { .fname = "symaddr",   .fptr = syslua_symaddr  },
    { .fname = "ccall",     .fptr = syslua_ccall    },
    { .fname = NULL,        .fptr = NULL            }
};

static int lua_modinit(
    lua_State *lua,
    const char *modname,
    const struct luamod_entry *entries)
{
    lua_newtable(lua);

    const struct luamod_entry *entry = entries;
    while (entry->fname) {
        lua_pushstring(lua, entry->fname);
        lua_pushcfunction(lua, entry->fptr);
        lua_settable(lua, -3);

        ++entry;
    }

    lua_setglobal(lua, modname);
    return 0;
}

void kshell_lua_test(void) {
    const char *prompt = "lua> ";
    char cmd_buf[CMD_SIZE];
    lua_State *lua;
    int luastack;

    logmsg("starting Lua...\n");
    vcsa_set_attribute(CONSOLE_VCSA, VCSA_ATTR_GREEN);
    k_printf("###\n###       Lua "LUA_VERSION_MAJOR"."LUA_VERSION_MINOR"\n###\n");
    k_printf("###  type 'q' to exit\n###\n");
    vcsa_set_attribute(CONSOLE_VCSA, VCSA_DEFAULT_ATTRIBUTE);

    lua = luaL_newstate();
    if (!lua)
        logmsge("luaL_newstate() -> NULL");

    lua_modinit(lua, "sys", luamod_sys);

    luaL_openlibs(lua);

    for (;;) {
        vcsa_set_attribute(CONSOLE_VCSA, BRIGHT(VCSA_ATTR_GREY));
        kshell_readline(cmd_buf, CMD_SIZE, prompt);
        vcsa_set_attribute(CONSOLE_VCSA, VCSA_DEFAULT_ATTRIBUTE);
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

