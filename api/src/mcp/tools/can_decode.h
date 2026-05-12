#pragma once
#include "../mcp_server.h"

namespace socketspy::api::mcp {

// can_decode: decode a raw CAN frame using a DBC file.
// Params: id (hex string), data (hex bytes string), dbc_file (path string)
// Returns: {message_name: string, signals: [{name, value, unit}]}
Tool make_can_decode_tool();

} // namespace socketspy::api::mcp
