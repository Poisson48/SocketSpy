#pragma once

struct lua_State;

namespace socketspy::scripting {

// Remove dangerous Lua standard library functions from the global environment.
// Called once per lua_State immediately after luaL_openlibs().
void lua_sandbox(lua_State* L);

} // namespace socketspy::scripting
