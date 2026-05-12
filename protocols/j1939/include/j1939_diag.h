#pragma once
#include <cstdint>
#include <vector>
#include <string_view>

namespace socketspy::protocols::j1939 {

struct Dtc {
    uint32_t spn;         // Suspect Parameter Number
    uint8_t  fmi;         // Failure Mode Identifier
    uint8_t  occurrence;
    bool     active;
};

// Decode DM1 (active faults) or DM2 (previously active) payload.
// data points to the reassembled multi-packet payload.
std::vector<Dtc> decode_dm1(const uint8_t* data, uint16_t len) noexcept;

std::string_view fmi_description(uint8_t fmi) noexcept;
std::string_view lamp_status_string(uint8_t lamp_byte) noexcept;

} // namespace socketspy::protocols::j1939
