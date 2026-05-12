#include "can_stop.h"

namespace socketspy::api::mcp {

Tool make_can_stop_tool() {
    Tool t;
    t.name        = "can_stop";
    t.description = "Cancel a periodic CAN frame transmission using the handle "
                    "returned by can_send. Also stops any active monitoring session.";

    t.input_schema = {
        {"type", "object"},
        {"properties", {
            {"handle", {
                {"type", "string"},
                {"description", "Transmission or monitor handle to cancel"}
            }}
        }},
        {"required", {"handle"}}
    };

    t.handler = [](const json& params) -> json {
        // TODO: wire to core handle registry
        (void)params;
        return {{"success", false}};
    };

    return t;
}

} // namespace socketspy::api::mcp
