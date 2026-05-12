#include "lua_can_api.h"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

namespace socketspy::scripting {

// ---------------------------------------------------------------------------
// Shared stub helper
// ---------------------------------------------------------------------------
static void push_not_connected(lua_State* L) {
    lua_getglobal(L, "log");
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, "print");
        if (lua_isfunction(L, -1)) {
            lua_pushstring(L, "can: not connected to core");
            lua_pcall(L, 1, 0, 0);
        } else {
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);
}

// ---------------------------------------------------------------------------
// can.* stubs
// ---------------------------------------------------------------------------

static int can_open(lua_State* L) {
    push_not_connected(L);
    lua_pushnil(L);
    return 1;
}

static int can_close(lua_State* L) {
    (void)L;
    return 0;
}

static int can_send(lua_State* L) {
    push_not_connected(L);
    lua_pushboolean(L, 0);
    return 1;
}

static int can_recv(lua_State* L) {
    push_not_connected(L);
    lua_pushnil(L);
    return 1;
}

static int can_filter(lua_State* L) {
    (void)L;
    return 0;
}

static int can_periodic(lua_State* L) {
    push_not_connected(L);
    lua_pushnil(L);
    return 1;
}

static int can_stop_periodic(lua_State* L) {
    (void)L;
    return 0;
}

static int can_set_fd(lua_State* L) {
    (void)L;
    return 0;
}

static const luaL_Reg kCanFuncs[] = {
    {"open",          can_open},
    {"close",         can_close},
    {"send",          can_send},
    {"recv",          can_recv},
    {"filter",        can_filter},
    {"periodic",      can_periodic},
    {"stop_periodic", can_stop_periodic},
    {"set_fd",        can_set_fd},
    {nullptr,         nullptr},
};

void register_can_api(lua_State* L) {
    lua_newtable(L);
    luaL_setfuncs(L, kCanFuncs, 0);
    lua_setglobal(L, "can");
}

// ---------------------------------------------------------------------------
// Generic "not connected" stub factory
// ---------------------------------------------------------------------------
static int proto_stub_nil(lua_State* L) {
    // Push a "not connected" message via log.print if available
    lua_getglobal(L, "log");
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, "print");
        if (lua_isfunction(L, -1)) {
            lua_pushstring(L, "protocol: not connected to core");
            lua_pcall(L, 1, 0, 0);
        } else {
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);
    lua_pushnil(L);
    return 1;
}

static int proto_stub_false(lua_State* L) {
    lua_getglobal(L, "log");
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, "print");
        if (lua_isfunction(L, -1)) {
            lua_pushstring(L, "protocol: not connected to core");
            lua_pcall(L, 1, 0, 0);
        } else {
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);
    lua_pushboolean(L, 0);
    return 1;
}

static int proto_stub_none(lua_State* L) {
    (void)L;
    return 0;
}

// ---------------------------------------------------------------------------
// Protocol tables
// ---------------------------------------------------------------------------

static const luaL_Reg kSdoFuncs[] = {
    {"read",  proto_stub_nil},
    {"write", proto_stub_false},
    {nullptr, nullptr},
};

static const luaL_Reg kNmtFuncs[] = {
    {"send",      proto_stub_none},
    {"wait_boot", proto_stub_nil},
    {nullptr,     nullptr},
};

static const luaL_Reg kPdoFuncs[] = {
    {"send",      proto_stub_none},
    {"subscribe", proto_stub_nil},
    {nullptr,     nullptr},
};

static const luaL_Reg kIsotpFuncs[] = {
    {"send",    proto_stub_nil},
    {"recv",    proto_stub_nil},
    {"request", proto_stub_nil},
    {nullptr,   nullptr},
};

static void register_table(lua_State* L, const char* name,
                            const luaL_Reg* funcs) {
    lua_newtable(L);
    luaL_setfuncs(L, funcs, 0);
    lua_setglobal(L, name);
}

void register_protocol_api(lua_State* L) {
    register_table(L, "sdo",   kSdoFuncs);
    register_table(L, "nmt",   kNmtFuncs);
    register_table(L, "pdo",   kPdoFuncs);
    register_table(L, "isotp", kIsotpFuncs);
}

} // namespace socketspy::scripting
