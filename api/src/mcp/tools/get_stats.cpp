#include "get_stats.h"

namespace socketspy::api::mcp {

Tool make_get_stats_tool() {
    Tool t;
    t.name        = "get_stats";
    t.description = "Retrieve live bus statistics from the SocketSpy core engine: "
                    "frame rate, bus load percentage, error frame count, number of "
                    "unique CAN IDs seen, and ring buffer utilisation.";

    t.input_schema = {
        {"type", "object"},
        {"properties", {
            {"iface", {
                {"type", "string"},
                {"description", "CAN interface to query (omit for aggregate stats)"}
            }}
        }},
        {"required", json::array()}
    };

    t.handler = [](const json& params) -> json {
        // TODO: read live counters from core ring buffer and stats module
        (void)params;
        return {
            {"fps",           0.0},
            {"bus_load_pct",  0.0},
            {"error_count",   0},
            {"unique_ids",    0},
            {"ring_used_pct", 0.0}
        };
    };

    return t;
}

} // namespace socketspy::api::mcp
