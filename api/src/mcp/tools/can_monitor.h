#pragma once
#include "../mcp_server.h"

namespace socketspy::api::mcp {

// can_monitor: capture CAN frames from an interface for a given duration.
// Params: iface (string), duration_ms (integer),
//         filter_id (optional hex string), decoded (optional bool)
// Returns: array of {ts_us, id, dlc, data_hex, signals?}
Tool make_can_monitor_tool();

} // namespace socketspy::api::mcp
