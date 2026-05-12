#pragma once
#include <cstdint>
#include <span>
#include <vector>

namespace socketspy::protocols::canopen {

// PDO COB-ID mapping entry
struct PdoMapping {
    uint32_t cob_id;
    uint8_t  num_mappings;
    struct SignalMap {
        uint16_t obj_index;
        uint8_t  subindex;
        uint8_t  bit_length;
    } signals[8];
};

// Decode a PDO frame given a mapping
struct PdoValue {
    uint16_t obj_index;
    uint8_t  subindex;
    uint64_t raw_value;
    uint8_t  bit_length;
};

bool decode_pdo(uint32_t can_id, const uint8_t* data, uint8_t dlc,
                const PdoMapping& mapping,
                PdoValue out_values[], uint8_t& num_out) noexcept;

} // namespace socketspy::protocols::canopen
