#pragma once
#include "../mcp_server.h"

namespace socketspy::api::mcp {

// get_stats: retrieve live bus statistics from the core engine.
// Params: iface (optional string)
// Returns: {fps, bus_load_pct, error_count, unique_ids, ring_used_pct}
Tool make_get_stats_tool();

} // namespace socketspy::api::mcp
