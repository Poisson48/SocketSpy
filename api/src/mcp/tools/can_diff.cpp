#include "can_diff.h"

namespace socketspy::api::mcp {

Tool make_can_diff_tool() {
    Tool t;
    t.name        = "can_diff";
    t.description = "Compare two CAN capture sessions and identify byte-level "
                    "differences. Useful for reverse engineering: capture a baseline, "
                    "trigger a function, capture again, then diff to find changed signals.";

    t.input_schema = {
        {"type", "object"},
        {"properties", {
            {"session_a", {
                {"type", "string"},
                {"description", "Path or session ID for the baseline capture"}
            }},
            {"session_b", {
                {"type", "string"},
                {"description", "Path or session ID for the comparison capture"}
            }},
            {"id_filter", {
                {"type", "string"},
                {"description", "Optional CAN ID filter in hex to narrow results"}
            }}
        }},
        {"required", {"session_a", "session_b"}}
    };

    t.handler = [](const json& params) -> json {
        // TODO: implement session diff engine
        (void)params;
        return json::array();
    };

    return t;
}

} // namespace socketspy::api::mcp
