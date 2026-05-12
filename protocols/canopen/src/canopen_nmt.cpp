#include "canopen_nmt.h"

namespace socketspy::protocols::canopen {

bool decode_nmt(uint32_t can_id, const uint8_t* data, uint8_t dlc,
                NmtMessage& out) noexcept {
    if (can_id != 0x000 || dlc < 2 || data == nullptr)
        return false;
    out.command = static_cast<NmtCommand>(data[0]);
    out.node_id = data[1];
    return true;
}

uint8_t encode_nmt(const NmtMessage& msg, uint8_t* out) noexcept {
    out[0] = static_cast<uint8_t>(msg.command);
    out[1] = msg.node_id;
    return 2;
}

bool decode_heartbeat(uint32_t can_id, const uint8_t* data, uint8_t dlc,
                      uint8_t& node_id_out, NmtState& state_out) noexcept {
    // Heartbeat COB-IDs live in range 0x701..0x77F
    if ((can_id & 0x780u) != 0x700u || (can_id & 0x7Fu) == 0)
        return false;
    if (dlc < 1 || data == nullptr)
        return false;
    node_id_out = static_cast<uint8_t>(can_id & 0x7Fu);
    const uint8_t raw = data[0] & 0x7Fu;  // bit 7 is toggle bit in old guarding protocol
    switch (raw) {
        case 0x00: state_out = NmtState::Initialising;   break;
        case 0x04: state_out = NmtState::Stopped;        break;
        case 0x05: state_out = NmtState::Operational;    break;
        case 0x7F: state_out = NmtState::PreOperational; break;
        default:   state_out = NmtState::Unknown;        break;
    }
    return true;
}

std::string_view nmt_state_name(NmtState s) noexcept {
    switch (s) {
        case NmtState::Initialising:   return "Initialising";
        case NmtState::Stopped:        return "Stopped";
        case NmtState::Operational:    return "Operational";
        case NmtState::PreOperational: return "PreOperational";
        default:                       return "Unknown";
    }
}

std::string_view nmt_command_name(NmtCommand c) noexcept {
    switch (c) {
        case NmtCommand::EnterOperational:    return "EnterOperational";
        case NmtCommand::EnterStop:           return "EnterStop";
        case NmtCommand::EnterPreOperational: return "EnterPreOperational";
        case NmtCommand::ResetNode:           return "ResetNode";
        case NmtCommand::ResetCommunication:  return "ResetCommunication";
        default:                              return "UnknownCommand";
    }
}

} // namespace socketspy::protocols::canopen
