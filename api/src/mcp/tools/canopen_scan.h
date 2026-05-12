#pragma once
#include "../mcp_server.h"

namespace socketspy::api::mcp {

// canopen_scan: discover all active CANopen nodes on the bus.
// Params: iface (string), timeout_ms (integer)
// Returns: array of {node_id, state, vendor_id, product_code}
Tool make_canopen_scan_tool();

} // namespace socketspy::api::mcp
