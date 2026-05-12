#include "canopen_emcy.h"
#include <cstring>

namespace socketspy::protocols::canopen {

bool decode_emcy(uint32_t can_id, const uint8_t* data, uint8_t dlc,
                 EmcyFrame& out) noexcept {
    // EMCY range: 0x081..0x0FF  (0x080 + node_id, node_id 1..127)
    if (can_id < 0x081u || can_id > 0x0FFu)
        return false;
    if (dlc < 8 || data == nullptr)
        return false;
    out.node_id        = static_cast<uint8_t>(can_id - 0x080u);
    out.error_code     = static_cast<uint16_t>(data[0] | (data[1] << 8));
    out.error_register = data[2];
    std::memcpy(out.vendor_data, data + 3, 5);
    return true;
}

std::string_view emcy_error_class(uint16_t error_code) noexcept {
    // Class is determined by the upper nibble / byte of the error code
    const uint16_t cls = error_code & 0xFF00u;
    switch (cls) {
        case 0x0000u: return "No error";
        case 0x1000u: return "Generic error";
        case 0x2000u: return "Current";
        case 0x3000u: return "Voltage";
        case 0x4000u: return "Temperature";
        case 0x5000u: return "Device hardware";
        case 0x6000u: return "Device software";
        case 0x7000u: return "Additional modules";
        case 0x8000u: return "Monitoring";
        case 0x9000u: return "External error";
        case 0xF000u: return "Device specific";
        default:      return "Reserved";
    }
}

} // namespace socketspy::protocols::canopen
