#include "can_decode.h"

namespace socketspy::api::mcp {

Tool make_can_decode_tool() {
    Tool t;
    t.name        = "can_decode";
    t.description = "Decode a raw CAN frame using a DBC signal database. "
                    "Returns the message name and an array of decoded signal "
                    "values with their engineering units.";

    t.input_schema = {
        {"type", "object"},
        {"properties", {
            {"id", {
                {"type", "string"},
                {"description", "CAN frame ID in hex, e.g. '0x123'"}
            }},
            {"data", {
                {"type", "string"},
                {"description", "Frame payload as hex bytes, e.g. 'DEADBEEF01020304'"}
            }},
            {"dbc_file", {
                {"type", "string"},
                {"description", "Absolute path to the .dbc signal database file"}
            }}
        }},
        {"required", {"id", "data", "dbc_file"}}
    };

    t.handler = [](const json& params) -> json {
        // TODO: integrate DBC parser and signal decoder
        (void)params;
        return {
            {"message_name", "unknown"},
            {"signals",      json::array()}
        };
    };

    return t;
}

} // namespace socketspy::api::mcp
