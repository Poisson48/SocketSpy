#pragma once
#include "../mcp_server.h"

namespace socketspy::api::mcp {

// can_replay: replay a captured CAN log file onto an interface.
// Params: log_file (string), iface (string), speed (number, multiplier)
// Returns: {status: string, frames_sent: integer, duration_ms: integer}
Tool make_can_replay_tool();

} // namespace socketspy::api::mcp
