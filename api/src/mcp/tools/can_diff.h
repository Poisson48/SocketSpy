#pragma once
#include "../mcp_server.h"

namespace socketspy::api::mcp {

// can_diff: compare two capture sessions and report changed byte positions.
// Params: session_a (string), session_b (string), id_filter (optional hex string)
// Returns: array of {id, byte_pos, value_a, value_b, count}
Tool make_can_diff_tool();

} // namespace socketspy::api::mcp
