#include "can_replay.h"

namespace socketspy::api::mcp {

Tool make_can_replay_tool() {
    Tool t;
    t.name        = "can_replay";
    t.description = "Replay a previously captured CAN log file onto a live interface. "
                    "Supports candump (.log) and SocketSpy native formats. "
                    "Speed multiplier controls playback rate (1.0 = real-time).";

    t.input_schema = {
        {"type", "object"},
        {"properties", {
            {"log_file", {
                {"type", "string"},
                {"description", "Absolute path to the CAN log file to replay"}
            }},
            {"iface", {
                {"type", "string"},
                {"description", "Target CAN interface name, e.g. vcan0"}
            }},
            {"speed", {
                {"type", "number"},
                {"description", "Playback speed multiplier (1.0 = real-time, 2.0 = 2x)"},
                {"minimum", 0.01},
                {"maximum", 100.0}
            }}
        }},
        {"required", {"log_file", "iface", "speed"}}
    };

    t.handler = [](const json& params) -> json {
        // TODO: implement log file parser and timed replay
        (void)params;
        return {
            {"status",      "not_implemented"},
            {"frames_sent", 0},
            {"duration_ms", 0}
        };
    };

    return t;
}

} // namespace socketspy::api::mcp
