#pragma once

struct lua_State;

namespace socketspy::scripting {

// Register bit.extract(byte, start, len) and bit.insert(byte, start, len, val).
// Register hex.to_bytes(str) -> table, hex.from_bytes(table) -> str.
void register_bit_api(lua_State* L);

} // namespace socketspy::scripting
