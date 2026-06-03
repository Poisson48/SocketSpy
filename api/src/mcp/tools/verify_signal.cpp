#include "verify_signal.h"

namespace socketspy::api::mcp {

Tool make_verify_signal_tool() {
    Tool t;
    t.name        = "verify_signal";
    t.description =
        "Confirm on the live CAN bus that a signal reaches an expected value at "
        "an operating point. This is the ECU-Studio -> SocketSpy live-verify hook: "
        "after a map is flashed, call it to wait until an optional condition signal "
        "is within tolerance of its operating-point value, then sample the target "
        "signal and pass iff |observed - target_value| <= tolerance before "
        "timeout_ms. Mirrors the SocketSpy Verify panel; returns a machine-readable "
        "pass/fail with the final observed value.";

    t.input_schema = {
        {"type", "object"},
        {"properties", {
            {"signal", {
                {"type", "string"},
                {"description", "Target signal name as defined in the DBC"}
            }},
            {"target_value", {
                {"type", "number"},
                {"description", "Expected physical value of the target signal"}
            }},
            {"tolerance", {
                {"type", "number"},
                {"description", "Absolute tolerance (>= 0) for the target and "
                                "condition checks"}
            }},
            {"condition_signal", {
                {"type", "string"},
                {"description", "Optional operating-point signal to gate on before "
                                "sampling the target"}
            }},
            {"condition_value", {
                {"type", "number"},
                {"description", "Operating-point value the condition signal must "
                                "reach (within tolerance)"}
            }},
            {"timeout_ms", {
                {"type", "integer"},
                {"description", "Maximum time to wait, in milliseconds (default 5000)"}
            }},
            {"dbc_file", {
                {"type", "string"},
                {"description", "Optional absolute path to the .dbc used to resolve "
                                "and decode the signals"}
            }}
        }},
        {"required", {"signal", "target_value", "tolerance"}}
    };

    t.handler = [](const json& params) -> json {
        // Validate the request shape up front so callers (ECU Studio) get a
        // clear, structured failure rather than an exception.
        if (!params.contains("signal") || !params["signal"].is_string()) {
            return {
                {"verified", false},
                {"final_value", nullptr},
                {"detail", "Missing or invalid 'signal' parameter."}
            };
        }
        if (!params.contains("target_value") || !params["target_value"].is_number()) {
            return {
                {"verified", false},
                {"final_value", nullptr},
                {"detail", "Missing or invalid 'target_value' parameter."}
            };
        }

        const std::string signal       = params["signal"].get<std::string>();
        const double      target       = params["target_value"].get<double>();
        double            tolerance    = params.value("tolerance", 0.0);
        if (tolerance < 0.0) tolerance = 0.0;
        const std::string conditionSig = params.value("condition_signal", std::string{});
        const double      conditionVal = params.value("condition_value", 0.0);
        const int         timeoutMs    = params.value("timeout_ms", 5000);

        // The actual bus interaction is performed by the running SocketSpy GUI's
        // VerifySignalPanel, which shares these verify semantics and owns the live
        // socket/DBC. This handler is the programmatic facade: it normalises the
        // request and forwards it. Until the GUI bridge transport is attached it
        // reports a structured "not connected" result so the API path is testable
        // and the contract is stable.
        const std::string condDesc = conditionSig.empty()
            ? std::string("immediately")
            : ("once '" + conditionSig + "' reaches " +
               std::to_string(conditionVal) + " within tolerance");

        json result = {
            {"verified",    false},
            {"final_value", nullptr},
            {"detail",
                "Verification of '" + signal + "' for target " +
                std::to_string(target) + " (+/- " + std::to_string(tolerance) +
                ") " + condDesc + ", timeout " + std::to_string(timeoutMs) +
                " ms — no live SocketSpy bridge attached."}
        };
        return result;
    };

    return t;
}

} // namespace socketspy::api::mcp
