#pragma once
#include "dbc_types.h"
#include <cstdint>
#include <span>
#include <optional>

namespace socketspy::dbc {

// Extract a signal value from raw CAN data bytes.
// data must be at least ceil((start_bit + bit_length) / 8) bytes long.
// Returns the physical value: raw * factor + offset
std::optional<double> extract_signal(const Signal& sig,
                                      std::span<const uint8_t> data) noexcept;

// Extract raw integer value (before factor/offset).
// Returns nullopt if data is too short.
std::optional<int64_t> extract_raw(const Signal& sig,
                                    std::span<const uint8_t> data) noexcept;

// Pack a physical value back into raw CAN data bytes.
// Returns false if value is out of [sig.min_val, sig.max_val].
bool pack_signal(const Signal& sig, double physical_value,
                 std::span<uint8_t> data) noexcept;

} // namespace socketspy::dbc
