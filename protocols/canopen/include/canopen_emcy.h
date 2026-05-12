#pragma once
#include <cstdint>
#include <string_view>

namespace socketspy::protocols::canopen {

// Decoded EMCY frame (CAN ID = 0x080 + node_id)
struct EmcyFrame {
    uint8_t  node_id;
    uint16_t error_code;
    uint8_t  error_register;
    uint8_t  vendor_data[5];
};

// Returns false if can_id is not in the EMCY range 0x081..0x0FF.
bool decode_emcy(uint32_t can_id, const uint8_t* data, uint8_t dlc,
                 EmcyFrame& out) noexcept;

std::string_view emcy_error_class(uint16_t error_code) noexcept;

} // namespace socketspy::protocols::canopen
