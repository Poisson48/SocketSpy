#include "can_send.h"

namespace socketspy::api::mcp {

Tool make_can_send_tool() {
    Tool t;
    t.name        = "can_send";
    t.description = "Transmit a CAN frame on the specified interface, either once "
                    "or at a periodic interval. Returns a handle for periodic frames "
                    "that can be cancelled with can_stop.";

    t.input_schema = {
        {"type", "object"},
        {"properties", {
            {"iface", {
                {"type", "string"},
                {"description", "CAN interface name, e.g. vcan0 or can0"}
            }},
            {"id", {
                {"type", "string"},
                {"description", "CAN frame ID in hex, e.g. '0x123'"}
            }},
            {"data", {
                {"type", "string"},
                {"description", "Frame payload as hex bytes, e.g. 'DEADBEEF01020304'"}
            }},
            {"periodic_ms", {
                {"type", "integer"},
                {"description", "If set, send frame repeatedly at this interval (ms)"},
                {"minimum", 1}
            }}
        }},
        {"required", {"iface", "id", "data"}}
    };

    t.handler = [](const json& params) -> json {
        // TODO: wire to core CAN sender
        (void)params;
        return {
            {"success", false},
            {"error",   "core not connected"}
        };
    };

    return t;
}

} // namespace socketspy::api::mcp
