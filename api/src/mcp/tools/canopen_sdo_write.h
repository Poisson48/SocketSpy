#pragma once
#include "../mcp_server.h"

namespace socketspy::api::mcp {

// canopen_sdo_write: write a value to a CANopen Object Dictionary entry via SDO.
// Params: iface (string), node_id (integer), index (hex string),
//         subindex (integer), value (any), type (string)
// Returns: {success: bool, error?: string}
Tool make_canopen_sdo_write_tool();

} // namespace socketspy::api::mcp
