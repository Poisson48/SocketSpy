#include "canopen_scan.h"

namespace socketspy::api::mcp {

Tool make_canopen_scan_tool() {
    Tool t;
    t.name        = "canopen_scan";
    t.description = "Scan a CAN bus for active CANopen nodes by sending NMT "
                    "node guarding or LSS requests and listening for heartbeat "
                    "messages. Returns node IDs, NMT states, and identity info.";

    t.input_schema = {
        {"type", "object"},
        {"properties", {
            {"iface", {
                {"type", "string"},
                {"description", "CAN interface name, e.g. vcan0 or can0"}
            }},
            {"timeout_ms", {
                {"type", "integer"},
                {"description", "Scan timeout in milliseconds"},
                {"minimum", 100},
                {"maximum", 10000}
            }}
        }},
        {"required", {"iface", "timeout_ms"}}
    };

    t.handler = [](const json& params) -> json {
        // TODO: implement CANopen node discovery
        (void)params;
        return json::array();
    };

    return t;
}

} // namespace socketspy::api::mcp
