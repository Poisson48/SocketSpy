#pragma once
#include <cstdint>
#include <functional>
#include <string_view>

namespace socketspy::protocols::canopen {

enum class NmtState : uint8_t {
    Initialising   = 0x00,
    Stopped        = 0x04,
    Operational    = 0x05,
    PreOperational = 0x7F,
    Unknown        = 0xFF,
};

enum class NmtCommand : uint8_t {
    EnterOperational    = 0x01,
    EnterStop           = 0x02,
    EnterPreOperational = 0x80,
    ResetNode           = 0x81,
    ResetCommunication  = 0x82,
};

struct NmtMessage {
    NmtCommand command;
    uint8_t    node_id;  // 0 = broadcast
};

// Decode NMT frame. Returns false if not an NMT frame (CAN ID != 0x000).
bool decode_nmt(uint32_t can_id, const uint8_t* data, uint8_t dlc,
                NmtMessage& out) noexcept;

// Encode NMT frame into 2 bytes. Returns DLC (always 2).
uint8_t encode_nmt(const NmtMessage& msg, uint8_t* out) noexcept;

// Decode heartbeat / boot-up frame.
// CAN ID = 0x700 + node_id, data[0] = state byte.
bool decode_heartbeat(uint32_t can_id, const uint8_t* data, uint8_t dlc,
                      uint8_t& node_id_out, NmtState& state_out) noexcept;

std::string_view nmt_state_name(NmtState s) noexcept;
std::string_view nmt_command_name(NmtCommand c) noexcept;

} // namespace socketspy::protocols::canopen
