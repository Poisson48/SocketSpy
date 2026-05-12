#pragma once
#include "../mcp_server.h"

namespace socketspy::api::mcp {

// can_send: transmit a CAN frame (once or periodically).
// Params: iface (string), id (hex string), data (hex bytes string),
//         periodic_ms (optional integer)
// Returns: {success: bool, handle?: string, error?: string}
Tool make_can_send_tool();

} // namespace socketspy::api::mcp
