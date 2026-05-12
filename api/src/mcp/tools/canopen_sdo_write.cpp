#include "canopen_sdo_write.h"

namespace socketspy::api::mcp {

Tool make_canopen_sdo_write_tool() {
    Tool t;
    t.name        = "canopen_sdo_write";
    t.description = "Write a value to a CANopen node's Object Dictionary entry "
                    "using the SDO protocol. The type parameter specifies how "
                    "to encode the value (e.g. uint8, int16, uint32, string).";

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
                {"description", "Object Dictionary index in hex, e.g. '0x1017'"}
            }},
            {"subindex", {
                {"type", "integer"},
                {"description", "Object Dictionary sub-index (0–255)"},
                {"minimum", 0},
                {"maximum", 255}
            }},
            {"value", {
                {"description", "Value to write (type must match 'type' parameter)"}
            }},
            {"type", {
                {"type", "string"},
                {"description", "Data type: uint8, int8, uint16, int16, uint32, int32, string"},
                {"enum", {"uint8","int8","uint16","int16","uint32","int32","string"}}
            }}
        }},
        {"required", {"iface", "node_id", "index", "subindex", "value", "type"}}
    };

    t.handler = [](const json& params) -> json {
        // TODO: implement CANopen SDO client write
        (void)params;
        return {{"success", false}};
    };

    return t;
}

} // namespace socketspy::api::mcp
