#pragma once
// Pure C++ (no Qt) helper for DBC operations.
// This header must NOT include any Qt headers, so that `signals` is never
// defined here and the dbc_types.h member `signals` is unambiguous.
#include "dbc_types.h"
#include "dbc_signal.h"
#include <cstdint>
#include <string>
#include <span>
#include <optional>

namespace socketspy::gui::dbc_helper {

// Decode all signals in a message matching `can_id` and return
// a formatted "name=value unit  name2=value2 unit2" string.
std::string decode_frame(const socketspy::dbc::DbcDatabase& db,
                         uint32_t can_id,
                         std::span<const uint8_t> data);

// Return the name of the first signal in the message matching `can_id`,
// or empty string if none.
std::string first_signal_name(const socketspy::dbc::DbcDatabase& db,
                              uint32_t can_id);

// Lookup signal min/max by name + msg id.
// Returns false if not found.
bool signal_range(const socketspy::dbc::DbcDatabase& db,
                  uint32_t msg_id,
                  const std::string& sig_name,
                  double& out_min, double& out_max);

// Decode a named signal value from a frame.
std::optional<double> decode_signal(const socketspy::dbc::DbcDatabase& db,
                                    uint32_t msg_id,
                                    const std::string& sig_name,
                                    std::span<const uint8_t> data);

} // namespace socketspy::gui::dbc_helper
