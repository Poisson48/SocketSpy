#include "lua_log_api.h"

#include <cstdio>
#include <cstring>
#include <string>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

namespace socketspy::scripting {

// Registry key to store the PrintCallback inside the Lua state.
static const char* kPrintCallbackKey = "socketspy.log.print_cb";

// ---------------------------------------------------------------------------
// log.print(msg)
// ---------------------------------------------------------------------------
static int log_print(lua_State* L) {
    // Accept any type; convert to string.
    const char* msg = nullptr;
    if (lua_isstring(L, 1)) {
        msg = lua_tostring(L, 1);
    } else {
        // Use luaL_tolstring which calls __tostring metamethod if available.
        msg = luaL_tolstring(L, 1, nullptr);
        lua_pop(L, 1); // luaL_tolstring pushes a copy
    }
    if (!msg) msg = "(nil)";

    // Retrieve the callback from the registry.
    lua_pushlightuserdata(L, reinterpret_cast<void*>(
        const_cast<char*>(kPrintCallbackKey)));
    lua_rawget(L, LUA_REGISTRYINDEX);
    auto* cb = static_cast<PrintCallback*>(lua_touserdata(L, -1));
    lua_pop(L, 1);

    if (cb && *cb) {
        (*cb)(msg);
    }
    return 0;
}

// ---------------------------------------------------------------------------
// log.csv(path, headers, row)
// headers and row are Lua array tables of strings/numbers.
// On first write (file does not exist / is empty) the headers row is written.
// ---------------------------------------------------------------------------
static int log_csv(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    luaL_checktype(L, 3, LUA_TTABLE);

    // Check whether file already exists and is non-empty.
    bool write_headers = false;
    {
        FILE* probe = std::fopen(path, "r");
        if (!probe) {
            write_headers = true;
        } else {
            std::fseek(probe, 0, SEEK_END);
            write_headers = (std::ftell(probe) == 0);
            std::fclose(probe);
        }
    }

    FILE* fp = std::fopen(path, "a");
    if (!fp) {
        return luaL_error(L, "log.csv: cannot open file '%s'", path);
    }

    auto write_table_row = [&](int idx) {
        lua_Integer n = luaL_len(L, idx);
        for (lua_Integer i = 1; i <= n; ++i) {
            if (i > 1) std::fputs(",", fp);
            lua_rawgeti(L, idx, i);
            if (lua_isstring(L, -1) || lua_isnumber(L, -1)) {
                std::fputs(lua_tostring(L, -1), fp);
            }
            lua_pop(L, 1);
        }
        std::fputs("\n", fp);
    };

    if (write_headers) {
        write_table_row(2);
    }
    write_table_row(3);
    std::fclose(fp);
    return 0;
}

static const luaL_Reg kLogFuncs[] = {
    {"print", log_print},
    {"csv",   log_csv},
    {nullptr, nullptr},
};

void register_log_api(lua_State* L, const PrintCallback& on_print) {
    // Heap-allocate the callback so it lives as long as the Lua state needs it.
    // We store it as a raw pointer in the registry backed by a full userdata
    // that owns the allocation.
    auto* cb_ptr = static_cast<PrintCallback*>(
        lua_newuserdata(L, sizeof(PrintCallback)));
    new (cb_ptr) PrintCallback(on_print); // placement new

    // Give it a __gc metamethod so the destructor is called when the state
    // closes.
    lua_newtable(L);
    lua_pushcfunction(L, [](lua_State* ls) -> int {
        auto* p = static_cast<PrintCallback*>(lua_touserdata(ls, 1));
        if (p) p->~PrintCallback();
        return 0;
    });
    lua_setfield(L, -2, "__gc");
    lua_setmetatable(L, -2);

    // Store in registry under kPrintCallbackKey.
    lua_pushlightuserdata(L, reinterpret_cast<void*>(
        const_cast<char*>(kPrintCallbackKey)));
    lua_pushvalue(L, -2); // push the full userdata again
    lua_rawset(L, LUA_REGISTRYINDEX);
    lua_pop(L, 1); // pop userdata

    // Register log table.
    lua_newtable(L);
    luaL_setfuncs(L, kLogFuncs, 0);
    lua_setglobal(L, "log");
}

} // namespace socketspy::scripting
