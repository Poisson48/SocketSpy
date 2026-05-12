#pragma once
#include <cstdint>
#include <functional>
#include <vector>
#include <optional>

namespace socketspy::protocols::isotp {

enum class IsoTpFrameType : uint8_t {
    Single       = 0,
    First        = 1,
    Consecutive  = 2,
    FlowControl  = 3,
};

IsoTpFrameType frame_type(const uint8_t* data) noexcept;

// Flow control flag values
enum class FcFlag : uint8_t {
    ContinueToSend = 0,
    Wait           = 1,
    Overflow       = 2,
};

// Decode a single-frame ISO-TP message (type=0).
// Returns payload length (0 on error).
uint8_t decode_single_frame(const uint8_t* data, uint8_t dlc,
                             uint8_t* payload_out, uint8_t payload_max) noexcept;

// Decode a first-frame. Returns total message length (0 on error).
uint16_t decode_first_frame(const uint8_t* data, uint8_t dlc,
                              uint8_t* first_payload_out,
                              uint8_t& first_payload_len) noexcept;

// Decode a consecutive frame. sn = sequence number 0..F.
uint8_t decode_consecutive_frame(const uint8_t* data, uint8_t dlc,
                                  uint8_t& sn_out,
                                  uint8_t* payload_out) noexcept;

// Encode a flow control frame. Returns DLC (3 bytes minimum).
uint8_t encode_flow_control(FcFlag flag, uint8_t block_size,
                              uint8_t st_min_ms, uint8_t* out8) noexcept;

// Stateful reassembler for a single ISO-TP channel.
using CompleteCallback = std::function<void(std::vector<uint8_t>)>;

class IsoTpReceiver {
public:
    explicit IsoTpReceiver(CompleteCallback on_complete);

    // Feed one CAN frame. Calls on_complete when message is fully received.
    // Returns true if the frame was consumed.
    bool feed(const uint8_t* data, uint8_t dlc);

    void reset() noexcept;

private:
    enum class State { Idle, Receiving };
    State state_{State::Idle};
    uint16_t expected_len_{0};
    uint8_t  next_sn_{1};
    std::vector<uint8_t> buf_;
    CompleteCallback on_complete_;
};

} // namespace socketspy::protocols::isotp
