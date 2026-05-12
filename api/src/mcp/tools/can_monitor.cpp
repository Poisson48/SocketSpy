#include "can_monitor.h"

namespace socketspy::api::mcp {

Tool make_can_monitor_tool() {
    Tool t;
    t.name        = "can_monitor";
    t.description = "Capture CAN frames from an interface for a given duration. "
                    "Returns an array of frame objects with timestamp, ID, DLC, "
                    "and raw hex data. Optionally filter by CAN ID and decode signals.";

    t.input_schema = {
        {"type", "object"},
        {"properties", {
            {"iface", {
                {"type", "string"},
                {"description", "CAN interface name, e.g. vcan0 or can0"}
            }},
            {"duration_ms", {
                {"type", "integer"},
                {"description", "Capture duration in milliseconds"},
                {"minimum", 1},
                {"maximum", 60000}
            }},
            {"filter_id", {
                {"type", "string"},
                {"description", "Optional CAN ID filter in hex, e.g. '0x123'"}
            }},
            {"decoded", {
                {"type", "boolean"},
                {"description", "If true, attempt signal decoding via loaded DBC"}
            }}
        }},
        {"required", {"iface", "duration_ms"}}
    };

    t.handler = [](const json& params) -> json {
        // TODO: connect to core ring buffer and vcan interface
        (void)params;
        return {
            {"frames", json::array()},
            {"note", "vcan not yet connected"}
        };
    };

    return t;
}

} // namespace socketspy::api::mcp
