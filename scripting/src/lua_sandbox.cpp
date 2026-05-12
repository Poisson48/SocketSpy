extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include "lua_sandbox.h"

namespace socketspy::scripting {

// Helper: set a nested field to nil. E.g. nil_field(L, "os", "execute")
// sets _G.os.execute = nil.
static void nil_field(lua_State* L, const char* table, const char* field) {
    lua_getglobal(L, table);
    if (lua_istable(L, -1)) {
        lua_pushnil(L);
        lua_setfield(L, -2, field);
    }
    lua_pop(L, 1);
}

// Helper: set a global to nil.
static void nil_global(lua_State* L, const char* name) {
    lua_pushnil(L);
    lua_setglobal(L, name);
}

void lua_sandbox(lua_State* L) {
    // -- os table: remove dangerous functions, keep time/clock/date --
    nil_field(L, "os", "execute");
    nil_field(L, "os", "exit");
    nil_field(L, "os", "getenv");
    nil_field(L, "os", "remove");
    nil_field(L, "os", "rename");
    nil_field(L, "os", "tmpname");

    // -- io table: remove popen, keep open/close/read/write --
    nil_field(L, "io", "popen");

    // -- debug table: keep traceback only --
    lua_getglobal(L, "debug");
    if (lua_istable(L, -1)) {
        // Save traceback before wiping
        lua_getfield(L, -1, "traceback");
        int has_tb = !lua_isnil(L, -1);
        lua_pop(L, 1);

        // Replace the entire debug table with a minimal one
        lua_newtable(L);
        if (has_tb) {
            lua_getglobal(L, "debug");
            lua_getfield(L, -1, "traceback");
            lua_remove(L, -2);
            lua_setfield(L, -2, "traceback");
        }
        lua_setglobal(L, "debug");
    } else {
        lua_pop(L, 1);
    }

    // -- Remove dynamic code loading --
    nil_global(L, "require");
    nil_global(L, "load");
    nil_global(L, "loadfile");
    nil_global(L, "dofile");
    nil_global(L, "loadstring");  // Lua 5.1 compat name, no-op in 5.4

    // -- Remove package system --
    nil_global(L, "package");

    // -- Remove rawget/rawset/rawequal/rawlen to prevent sandbox escapes --
    // (Keep them — they are needed for legitimate table manipulation;
    //  the real danger is dynamic loading, not raw access.)

    // -- Remove collectgarbage (can be used as a timing oracle / DoS) --
    nil_global(L, "collectgarbage");
}

} // namespace socketspy::scripting
