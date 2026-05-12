#pragma once
#include <cstdint>
#include <string_view>
#include <optional>

namespace socketspy::protocols::obd2 {

struct Obd2Response {
    uint8_t mode;
    uint8_t pid;
    uint8_t data[4];
    uint8_t data_len;
};

// Decode a raw OBD-II response frame.
// Returns std::nullopt if not a valid OBD-II response.
std::optional<Obd2Response> decode_obd2(const uint8_t* data, uint8_t dlc) noexcept;

// Interpret mode 01 PID value. Returns physical value as double.
// e.g. pid 0x0C = RPM = (256*A + B) / 4
std::optional<double> interpret_pid(uint8_t mode, uint8_t pid,
                                     const uint8_t* data, uint8_t len) noexcept;

std::string_view pid_name(uint8_t mode, uint8_t pid) noexcept;
std::string_view pid_unit(uint8_t mode, uint8_t pid) noexcept;

} // namespace socketspy::protocols::obd2
