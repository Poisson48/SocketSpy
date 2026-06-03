#pragma once
#include "../mcp_server.h"

namespace socketspy::api::mcp {

// verify_signal: the programmatic entry point for the ECU-Studio -> SocketSpy
// live-verify hook. After ECU Studio flashes a map it calls this tool to confirm
// on the live bus that a signal reaches an expected value at an operating point.
//
// Params:
//   signal           (string)  target signal name (must exist in the DBC)
//   target_value     (number)  expected physical value of the target signal
//   tolerance        (number)  absolute tolerance for both the target and the
//                              optional condition check (>= 0)
//   condition_signal (string, optional) operating-point signal to gate on
//   condition_value  (number, optional) operating-point value to wait for
//   timeout_ms       (integer, optional, default 5000) max wall time
//   dbc_file         (string, optional) DBC used to resolve / decode signals
//
// Returns:
//   {verified: bool, final_value: number|null, detail: string}
//
// This mirrors the VerifySignalPanel verify semantics in the GUI: wait until the
// condition signal is within tolerance of condition_value, then sample the
// target; pass iff |observed - target_value| <= tolerance within timeout_ms.
Tool make_verify_signal_tool();

} // namespace socketspy::api::mcp
