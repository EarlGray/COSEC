#include <string.h>

#include <cosec/log.h>

#include <lua/lua.h>
#include <lua/lualib.h>
#include <lua/lauxlib.h>


#define CMD_SIZE 512

int main() {
    const char *prompt = "lua> ";
    char cmd_buf[CMD_SIZE];
    lua_State *lua;

    logmsg("starting Lua...\n");
    k_printf("###\n###       Lua "LUA_VERSION_MAJOR"."LUA_VERSION_MINOR"\n###\n");
    k_printf("###  type 'q' to exit\n###\n");

    lua = luaL_newstate();
    if (!lua)
        logmsge("luaL_newstate() -> NULL");

    luaL_openlibs(lua);

    const char *custom = "_ENV['dir'] = function(t) for k, v in pairs(t) do print(k, v) end end";
    int err = luaL_loadbuffer(lua, custom, strlen(custom), "<cosec>");
    if (err != LUA_OK) {
        logmsgef("### Error: %s\n", lua_tostring(lua, -1));
        lua_pop(lua, 1);
    } else {
        if ((err = lua_pcall(lua, 0, 0, 0)) != LUA_OK) {
            fprintf(stderr, "### Exception: %s\n", lua_tostring(lua, -1));
            lua_pop(lua, 1);
        }
    }

    for (;;) {
        printf("%s", prompt);
        fgets(cmd_buf, CMD_SIZE, stdin);
        if (!strcasecmp(cmd_buf, "q"))
            break;

        int err = luaL_loadbuffer(lua, cmd_buf, strlen(cmd_buf), "*input*");
        if (err != LUA_OK) {
            fprintf(stderr, "### Error: %s\n", luaL_checkstring(lua, -1));
            continue;
        }

        err = lua_pcall(lua, 0, 0, 0);
        if (err != LUA_OK) {
            fprintf(stderr, "### Exception: %s\n", lua_tostring(lua, -1));
            lua_pop(lua, 1);
        }
    }

    lua_close(lua);
}
