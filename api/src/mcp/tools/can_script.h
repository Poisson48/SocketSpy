#pragma once
#include "../mcp_server.h"

namespace socketspy::api::mcp {

// can_script: execute a Lua script in the embedded SocketSpy scripting engine.
// Params: lua_code (string), timeout_ms (optional integer)
// Returns: {stdout: string, result: string, error?: string, elapsed_ms: integer}
Tool make_can_script_tool();

} // namespace socketspy::api::mcp
