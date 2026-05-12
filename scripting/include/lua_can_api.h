#pragma once

struct lua_State;

namespace socketspy::scripting {

// Register the 'can' table into the Lua state.
// After this call, scripts can call can.open(), can.send(), can.recv(), etc.
// All stubs at this stage — they print a "not connected" message and return nil.
void register_can_api(lua_State* L);

// Register the 'sdo', 'nmt', 'pdo', 'isotp' tables (protocol-level stubs).
void register_protocol_api(lua_State* L);

} // namespace socketspy::scripting
