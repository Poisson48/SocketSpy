#include "canopen_sdo.h"
#include <cstring>

namespace socketspy::protocols::canopen {

// SDO command specifier nibbles (upper 3 bits of data[0])
static constexpr uint8_t CS_UPLOAD_INIT_REQ  = 0x40;
static constexpr uint8_t CS_UPLOAD_INIT_RSP  = 0x43; // expedited, size indicated
static constexpr uint8_t CS_UPLOAD_SEG_RSP   = 0x60;
static constexpr uint8_t CS_DOWNLOAD_INIT_RSP= 0x60;
static constexpr uint8_t CS_DOWNLOAD_SEG_RSP = 0x20;
static constexpr uint8_t CS_ABORT            = 0x80;
static constexpr uint8_t CS_BLOCK_UPLOAD_INIT= 0xC0;

std::string_view SdoResult::error_msg() const noexcept {
    return sdo_abort_string(abort_code);
}

bool decode_sdo_response(uint32_t can_id, uint8_t node_id,
                         const uint8_t* payload, uint8_t dlc,
                         SdoFrame& out) noexcept {
    if (can_id != static_cast<uint32_t>(0x580u + node_id))
        return false;
    if (dlc < 4 || payload == nullptr)
        return false;

    const uint8_t cs = payload[0];
    out.index    = static_cast<uint16_t>(payload[1] | (payload[2] << 8));
    out.subindex = payload[3];
    out.abort_code  = 0;
    out.toggle      = false;
    out.last_segment = false;
    out.data_len    = 0;
    std::memset(out.data, 0, sizeof(out.data));

    const uint8_t upper3 = cs & 0xE0u;
    const uint8_t lower5 = cs & 0x1Fu;

    if (cs == CS_ABORT) {
        out.type = SdoResponseType::AbortTransfer;
        if (dlc >= 8)
            out.abort_code = static_cast<uint32_t>(payload[4])
                           | (static_cast<uint32_t>(payload[5]) << 8)
                           | (static_cast<uint32_t>(payload[6]) << 16)
                           | (static_cast<uint32_t>(payload[7]) << 24);
        return true;
    }

    if (upper3 == 0x40u) {
        // Expedited or segmented upload initiate response
        const bool expedited   = (cs >> 1) & 1u;
        const bool size_ind    = cs & 1u;
        if (expedited) {
            out.type = SdoResponseType::InitiateUploadExpedited;
            // bits 3..2 = number of bytes NOT used (only valid when size_ind=1)
            uint8_t unused = size_ind ? ((cs >> 2) & 0x03u) : 0;
            out.data_len = static_cast<uint8_t>(4u - unused);
            if (dlc >= 8)
                std::memcpy(out.data, payload + 4, out.data_len);
        } else {
            out.type = SdoResponseType::InitiateUploadSegmented;
            // Total size is in payload[4..7]
            out.data_len = 4;
            if (dlc >= 8)
                std::memcpy(out.data, payload + 4, 4);
        }
        return true;
    }

    if (upper3 == 0x00u) {
        // Upload segment response  cs = 0x0x
        out.type = SdoResponseType::UploadSegment;
        out.toggle      = (cs >> 4) & 1u;
        out.last_segment = cs & 1u;
        uint8_t unused   = (cs >> 1) & 0x07u;
        out.data_len     = static_cast<uint8_t>(7u - unused);
        if (dlc >= 8)
            std::memcpy(out.data, payload + 1, out.data_len > 4 ? 4 : out.data_len);
        return true;
    }

    if (upper3 == 0x60u && lower5 == 0x00u) {
        // Download initiate ack: 0x60
        out.type = SdoResponseType::InitiateDownloadAck;
        return true;
    }

    if (upper3 == 0x20u) {
        // Download segment ack
        out.type = SdoResponseType::DownloadSegmentAck;
        out.toggle = (cs >> 4) & 1u;
        return true;
    }

    if (upper3 == 0xC0u) {
        out.type = SdoResponseType::BlockUploadInitiate;
        return true;
    }

    out.type = SdoResponseType::Unknown;
    return true;
}

uint8_t build_sdo_upload_request(uint16_t index, uint8_t subindex,
                                  uint8_t* out8) noexcept {
    out8[0] = CS_UPLOAD_INIT_REQ;
    out8[1] = static_cast<uint8_t>(index & 0xFFu);
    out8[2] = static_cast<uint8_t>(index >> 8);
    out8[3] = subindex;
    out8[4] = 0; out8[5] = 0; out8[6] = 0; out8[7] = 0;
    return 8;
}

uint8_t build_sdo_download_expedited(uint16_t index, uint8_t subindex,
                                      const uint8_t* value, uint8_t vlen,
                                      uint8_t* out8) noexcept {
    if (vlen > 4) vlen = 4;
    // cs: 0x23 | ((4-vlen) & 3) << 2 | size_indicated=1 | expedited=1
    out8[0] = static_cast<uint8_t>(0x23u | (static_cast<uint8_t>((4u - vlen) & 0x03u) << 2));
    out8[1] = static_cast<uint8_t>(index & 0xFFu);
    out8[2] = static_cast<uint8_t>(index >> 8);
    out8[3] = subindex;
    out8[4] = 0; out8[5] = 0; out8[6] = 0; out8[7] = 0;
    for (uint8_t i = 0; i < vlen; ++i)
        out8[4 + i] = value[i];
    return 8;
}

uint8_t build_sdo_abort(uint16_t index, uint8_t subindex,
                         uint32_t abort_code, uint8_t* out8) noexcept {
    out8[0] = CS_ABORT;
    out8[1] = static_cast<uint8_t>(index & 0xFFu);
    out8[2] = static_cast<uint8_t>(index >> 8);
    out8[3] = subindex;
    out8[4] = static_cast<uint8_t>(abort_code & 0xFFu);
    out8[5] = static_cast<uint8_t>((abort_code >> 8)  & 0xFFu);
    out8[6] = static_cast<uint8_t>((abort_code >> 16) & 0xFFu);
    out8[7] = static_cast<uint8_t>((abort_code >> 24) & 0xFFu);
    return 8;
}

std::string_view sdo_abort_string(uint32_t code) noexcept {
    switch (code) {
        case 0x05030000u: return "Toggle bit not alternated";
        case 0x05040000u: return "SDO protocol timed out";
        case 0x05040001u: return "Client/server command specifier unknown";
        case 0x05040002u: return "Invalid block size";
        case 0x05040003u: return "Invalid sequence number";
        case 0x05040004u: return "CRC error";
        case 0x05040005u: return "Out of memory";
        case 0x06010000u: return "Unsupported access to object";
        case 0x06010001u: return "Attempt to read a write-only object";
        case 0x06010002u: return "Attempt to write a read-only object";
        case 0x06020000u: return "Object does not exist";
        case 0x06040041u: return "Object cannot be mapped to PDO";
        case 0x06040042u: return "Number and length of mapped objects exceed PDO length";
        case 0x06040043u: return "General parameter incompatibility";
        case 0x06040047u: return "General internal incompatibility";
        case 0x06060000u: return "Hardware error";
        case 0x06070010u: return "Data type mismatch, length too high";
        case 0x06070012u: return "Data type mismatch, length too low";
        case 0x06090011u: return "Sub-index does not exist";
        case 0x06090030u: return "Value range exceeded";
        case 0x06090031u: return "Value too high";
        case 0x06090032u: return "Value too low";
        case 0x06090036u: return "Maximum less than minimum";
        case 0x08000000u: return "General error";
        case 0x08000020u: return "Data cannot be transferred or stored";
        case 0x08000021u: return "Data cannot be transferred due to local control";
        case 0x08000022u: return "Data cannot be transferred due to device state";
        case 0x08000023u: return "Object dictionary dynamic generation failed";
        default:          return "Unknown abort code";
    }
}

} // namespace socketspy::protocols::canopen
