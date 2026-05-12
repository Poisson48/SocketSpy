#pragma once
#include "../mcp_server.h"

namespace socketspy::api::mcp {

// canopen_sdo_read: read a CANopen object dictionary entry via SDO.
// Params: iface (string), node_id (integer), index (hex string), subindex (integer)
// Returns: {value: any, type: string, raw_hex: string, error?: string}
Tool make_canopen_sdo_read_tool();

} // namespace socketspy::api::mcp
