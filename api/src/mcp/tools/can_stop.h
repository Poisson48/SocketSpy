#pragma once
#include "../mcp_server.h"

namespace socketspy::api::mcp {

// can_stop: cancel a periodic CAN transmission by handle.
// Params: handle (string)  — handle returned by can_send
// Returns: {success: bool}
Tool make_can_stop_tool();

} // namespace socketspy::api::mcp
