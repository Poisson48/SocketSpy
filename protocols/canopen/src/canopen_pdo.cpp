#include "canopen_pdo.h"

namespace socketspy::protocols::canopen {

bool decode_pdo(uint32_t can_id, const uint8_t* data, uint8_t dlc,
                const PdoMapping& mapping,
                PdoValue out_values[], uint8_t& num_out) noexcept {
    if (can_id != mapping.cob_id)
        return false;
    if (data == nullptr || mapping.num_mappings == 0)
        return false;

    num_out = 0;
    uint8_t bit_offset = 0;

    for (uint8_t i = 0; i < mapping.num_mappings; ++i) {
        const auto& sig = mapping.signals[i];
        if (sig.bit_length == 0)
            continue;

        // Check that the bits are within the received data
        uint8_t end_bit = static_cast<uint8_t>(bit_offset + sig.bit_length);
        if (end_bit > static_cast<uint16_t>(dlc) * 8u)
            break;

        // LSB-first extraction from the data byte array
        uint64_t value = 0;
        for (uint8_t b = 0; b < sig.bit_length; ++b) {
            uint8_t abs_bit  = static_cast<uint8_t>(bit_offset + b);
            uint8_t byte_idx = abs_bit / 8u;
            uint8_t bit_in_byte = abs_bit % 8u;
            if ((data[byte_idx] >> bit_in_byte) & 1u)
                value |= (UINT64_C(1) << b);
        }

        out_values[num_out++] = PdoValue{
            .obj_index  = sig.obj_index,
            .subindex   = sig.subindex,
            .raw_value  = value,
            .bit_length = sig.bit_length,
        };
        bit_offset = static_cast<uint8_t>(bit_offset + sig.bit_length);
    }
    return num_out > 0;
}

} // namespace socketspy::protocols::canopen
