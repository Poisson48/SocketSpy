#include "canopen_sdo_read.h"

namespace socketspy::api::mcp {

Tool make_canopen_sdo_read_tool() {
    Tool t;
    t.name        = "canopen_sdo_read";
    t.description = "Read a single entry from a CANopen node's Object Dictionary "
                    "using the Service Data Object (SDO) protocol. Supports "
                    "expedited and segmented transfers.";

    t.input_schema = {
        {"type", "object"},
        {"properties", {
            {"iface", {
                {"type", "string"},
                {"description", "CAN interface name, e.g. vcan0 or can0"}
            }},
            {"node_id", {
                {"type", "integer"},
                {"description", "CANopen node ID (1–127)"},
                {"minimum", 1},
                {"maximum", 127}
            }},
            {"index", {
                {"type", "string"},
                {"description", "Object Dictionary index in hex, e.g. '0x1018'"}
            }},
            {"subindex", {
                {"type", "integer"},
                {"description", "Object Dictionary sub-index (0–255)"},
                {"minimum", 0},
                {"maximum", 255}
            }}
        }},
        {"required", {"iface", "node_id", "index", "subindex"}}
    };

    t.handler = [](const json& params) -> json {
        // TODO: implement CANopen SDO client
        (void)params;
        return {
            {"value",   nullptr},
            {"type",    "unknown"},
            {"raw_hex", ""}
        };
    };

    return t;
}

} // namespace socketspy::api::mcp
