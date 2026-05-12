#include "can_script.h"

namespace socketspy::api::mcp {

Tool make_can_script_tool() {
    Tool t;
    t.name        = "can_script";
    t.description = "Execute a Lua script inside the SocketSpy scripting engine. "
                    "Scripts have access to CAN send/receive APIs and all loaded "
                    "signal databases. Returns captured stdout, final result, and "
                    "elapsed execution time.";

    t.input_schema = {
        {"type", "object"},
        {"properties", {
            {"lua_code", {
                {"type", "string"},
                {"description", "Lua source code to execute"}
            }},
            {"timeout_ms", {
                {"type", "integer"},
                {"description", "Maximum execution time in milliseconds (default: 5000)"},
                {"minimum", 100},
                {"maximum", 30000}
            }}
        }},
        {"required", {"lua_code"}}
    };

    t.handler = [](const json& params) -> json {
        // TODO: integrate Lua 5.4 scripting engine
        (void)params;
        return {
            {"stdout",     ""},
            {"result",     "null"},
            {"elapsed_ms", 0}
        };
    };

    return t;
}

} // namespace socketspy::api::mcp
