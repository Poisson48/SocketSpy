#pragma once
#include <cstdint>
#include <span>
#include <vector>
#include <optional>
#include <string_view>

namespace socketspy::protocols::canopen {

// SDO result (value as raw bytes)
struct SdoResult {
    std::vector<uint8_t> data;
    bool                 ok;
    uint32_t             abort_code;  // non-zero if ok==false
    std::string_view     error_msg() const noexcept;
};

// Decode an SDO response frame into its type and payload.
// rx_cob_id should be 0x580 + node_id for server responses.
enum class SdoResponseType : uint8_t {
    InitiateUploadExpedited,
    InitiateUploadSegmented,
    UploadSegment,
    InitiateDownloadAck,
    DownloadSegmentAck,
    BlockUploadInitiate,
    AbortTransfer,
    Unknown,
};

struct SdoFrame {
    SdoResponseType type;
    uint16_t        index;
    uint8_t         subindex;
    uint8_t         data[4];
    uint8_t         data_len;  // valid bytes in data[]
    uint32_t        abort_code;
    bool            toggle;
    bool            last_segment;
};

bool decode_sdo_response(uint32_t can_id, uint8_t node_id,
                         const uint8_t* payload, uint8_t dlc,
                         SdoFrame& out) noexcept;

// Build SDO expedited upload request (read). Returns DLC (8 bytes).
uint8_t build_sdo_upload_request(uint16_t index, uint8_t subindex,
                                  uint8_t* out8) noexcept;

// Build SDO expedited download request (write, ≤4 bytes).
uint8_t build_sdo_download_expedited(uint16_t index, uint8_t subindex,
                                      const uint8_t* value, uint8_t vlen,
                                      uint8_t* out8) noexcept;

// Build SDO abort frame.
uint8_t build_sdo_abort(uint16_t index, uint8_t subindex,
                         uint32_t abort_code, uint8_t* out8) noexcept;

// Look up human-readable SDO abort code string.
std::string_view sdo_abort_string(uint32_t code) noexcept;

} // namespace socketspy::protocols::canopen
