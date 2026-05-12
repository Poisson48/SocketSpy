#pragma once
#include <cstdint>
#include <string_view>

namespace socketspy::protocols::j1939 {

// J1939 29-bit CAN ID decomposition
struct J1939Id {
    uint8_t  priority;    // bits 28-26
    bool     reserved;    // bit 25
    bool     data_page;   // bit 24
    uint8_t  pf;          // PDU Format byte (bits 23-16)
    uint8_t  ps;          // PDU Specific (bits 15-8): DA if pf<240, group ext if pf>=240
    uint8_t  sa;          // Source Address (bits 7-0)
    uint32_t pgn;         // reconstructed PGN
    bool     peer_to_peer;// true if pf < 0xF0
};

J1939Id decode_j1939_id(uint32_t can_id) noexcept;
uint32_t encode_j1939_id(const J1939Id& id) noexcept;

// Look up PGN name from a static table (~50 common PGNs).
std::string_view pgn_name(uint32_t pgn) noexcept;

// Look up source address / industry group name.
std::string_view address_claim_name(uint8_t sa) noexcept;

} // namespace socketspy::protocols::j1939
