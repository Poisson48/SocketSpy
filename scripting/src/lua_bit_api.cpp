#include "lua_bit_api.h"

#include <cctype>
#include <cstdint>
#include <string>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

namespace socketspy::scripting {

// ---------------------------------------------------------------------------
// bit.extract(value, start, len) -> integer
// Extracts `len` bits starting at bit `start` from `value`.
// ---------------------------------------------------------------------------
static int bit_extract(lua_State* L) {
    lua_Integer value = luaL_checkinteger(L, 1);
    lua_Integer start = luaL_checkinteger(L, 2);
    lua_Integer len   = luaL_checkinteger(L, 3);

    if (start < 0 || len < 0 || len > 64 || start + len > 64) {
        return luaL_error(L, "bit.extract: out-of-range start=%lld len=%lld",
                          (long long)start, (long long)len);
    }
    lua_Integer mask   = (len == 64) ? ~(lua_Integer)0
                                     : ((lua_Integer)1 << len) - 1;
    lua_Integer result = (value >> start) & mask;
    lua_pushinteger(L, result);
    return 1;
}

// ---------------------------------------------------------------------------
// bit.insert(value, start, len, new_bits) -> integer
// Clears bits [start, start+len) in `value` then ORs in `new_bits << start`.
// ---------------------------------------------------------------------------
static int bit_insert(lua_State* L) {
    lua_Integer value    = luaL_checkinteger(L, 1);
    lua_Integer start    = luaL_checkinteger(L, 2);
    lua_Integer len      = luaL_checkinteger(L, 3);
    lua_Integer new_bits = luaL_checkinteger(L, 4);

    if (start < 0 || len < 0 || len > 64 || start + len > 64) {
        return luaL_error(L, "bit.insert: out-of-range start=%lld len=%lld",
                          (long long)start, (long long)len);
    }
    lua_Integer mask   = (len == 64) ? ~(lua_Integer)0
                                     : ((lua_Integer)1 << len) - 1;
    lua_Integer result = (value & ~(mask << start)) | ((new_bits & mask) << start);
    lua_pushinteger(L, result);
    return 1;
}

// ---------------------------------------------------------------------------
// hex.to_bytes(str) -> table of integers
// Parses a hex string (spaces ignored, optional "0x"/"0X" prefix).
// ---------------------------------------------------------------------------
static int hex_to_bytes(lua_State* L) {
    size_t      len = 0;
    const char* s   = luaL_checklstring(L, 1, &len);

    // Skip optional "0x" or "0X" prefix.
    size_t i = 0;
    if (len >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        i = 2;
    }

    lua_newtable(L);
    int table_idx = 1;

    // Collect hex digits (ignore spaces).
    std::string digits;
    digits.reserve(len);
    for (; i < len; ++i) {
        if (std::isspace(static_cast<unsigned char>(s[i]))) continue;
        if (!std::isxdigit(static_cast<unsigned char>(s[i]))) {
            return luaL_error(L, "hex.to_bytes: invalid character '%c'", s[i]);
        }
        digits += s[i];
    }

    // Pad to even length.
    if (digits.size() % 2 != 0) digits = "0" + digits;

    for (size_t j = 0; j < digits.size(); j += 2) {
        unsigned int byte_val = 0;
        sscanf(digits.c_str() + j, "%2x", &byte_val);  // NOLINT
        lua_pushinteger(L, static_cast<lua_Integer>(byte_val));
        lua_rawseti(L, -2, table_idx++);
    }
    return 1;
}

// ---------------------------------------------------------------------------
// hex.from_bytes(table) -> string
// Converts a Lua array of integers to a lowercase hex string.
// ---------------------------------------------------------------------------
static int hex_from_bytes(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_Integer n = luaL_len(L, 1);

    std::string out;
    out.reserve(static_cast<size_t>(n) * 2);

    char buf[3];
    for (lua_Integer i = 1; i <= n; ++i) {
        lua_rawgeti(L, 1, i);
        lua_Integer val = luaL_checkinteger(L, -1);
        lua_pop(L, 1);
        snprintf(buf, sizeof(buf), "%02x",  // NOLINT
                 static_cast<unsigned>(val & 0xFF));
        out += buf;
    }
    lua_pushlstring(L, out.c_str(), out.size());
    return 1;
}

static const luaL_Reg kBitFuncs[] = {
    {"extract", bit_extract},
    {"insert",  bit_insert},
    {nullptr,   nullptr},
};

static const luaL_Reg kHexFuncs[] = {
    {"to_bytes",   hex_to_bytes},
    {"from_bytes", hex_from_bytes},
    {nullptr,      nullptr},
};

void register_bit_api(lua_State* L) {
    lua_newtable(L);
    luaL_setfuncs(L, kBitFuncs, 0);
    lua_setglobal(L, "bit");

    lua_newtable(L);
    luaL_setfuncs(L, kHexFuncs, 0);
    lua_setglobal(L, "hex");
}

} // namespace socketspy::scripting
