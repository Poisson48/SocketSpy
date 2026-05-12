#include "isotp.h"
#include <cstring>
#include <algorithm>

namespace socketspy::protocols::isotp {

IsoTpFrameType frame_type(const uint8_t* data) noexcept {
    return static_cast<IsoTpFrameType>((data[0] >> 4) & 0x0Fu);
}

uint8_t decode_single_frame(const uint8_t* data, uint8_t dlc,
                             uint8_t* payload_out, uint8_t payload_max) noexcept {
    if (data == nullptr || dlc < 2)
        return 0;
    if ((data[0] >> 4) != 0)  // must be single-frame (type 0)
        return 0;
    uint8_t plen = data[0] & 0x0Fu;
    if (plen == 0 || plen > 7)
        return 0;
    if (static_cast<uint8_t>(dlc - 1) < plen)
        return 0;
    uint8_t copy = plen < payload_max ? plen : payload_max;
    std::memcpy(payload_out, data + 1, copy);
    return plen;
}

uint16_t decode_first_frame(const uint8_t* data, uint8_t dlc,
                              uint8_t* first_payload_out,
                              uint8_t& first_payload_len) noexcept {
    if (data == nullptr || dlc < 2)
        return 0;
    if ((data[0] >> 4) != 1)  // must be first-frame (type 1)
        return 0;
    uint16_t total = static_cast<uint16_t>(((data[0] & 0x0Fu) << 8) | data[1]);
    if (total == 0)
        return 0;
    // First frame carries 6 bytes of payload (bytes 2..7)
    first_payload_len = (dlc >= 8) ? 6u : static_cast<uint8_t>(dlc - 2);
    if (first_payload_out != nullptr)
        std::memcpy(first_payload_out, data + 2, first_payload_len);
    return total;
}

uint8_t decode_consecutive_frame(const uint8_t* data, uint8_t dlc,
                                  uint8_t& sn_out,
                                  uint8_t* payload_out) noexcept {
    if (data == nullptr || dlc < 2)
        return 0;
    if ((data[0] >> 4) != 2)  // must be consecutive-frame (type 2)
        return 0;
    sn_out = data[0] & 0x0Fu;
    uint8_t plen = static_cast<uint8_t>(dlc - 1u);
    if (payload_out != nullptr)
        std::memcpy(payload_out, data + 1, plen);
    return plen;
}

uint8_t encode_flow_control(FcFlag flag, uint8_t block_size,
                              uint8_t st_min_ms, uint8_t* out8) noexcept {
    out8[0] = static_cast<uint8_t>(0x30u | (static_cast<uint8_t>(flag) & 0x0Fu));
    out8[1] = block_size;
    out8[2] = st_min_ms;
    return 3;
}

// ---------------------------------------------------------------------------
// IsoTpReceiver
// ---------------------------------------------------------------------------

IsoTpReceiver::IsoTpReceiver(CompleteCallback on_complete)
    : on_complete_(std::move(on_complete)) {}

void IsoTpReceiver::reset() noexcept {
    state_        = State::Idle;
    expected_len_ = 0;
    next_sn_      = 1;
    buf_.clear();
}

bool IsoTpReceiver::feed(const uint8_t* data, uint8_t dlc) {
    if (data == nullptr || dlc < 1)
        return false;

    const IsoTpFrameType ft = frame_type(data);

    switch (ft) {
        case IsoTpFrameType::Single: {
            uint8_t tmp[7]{};
            uint8_t plen = decode_single_frame(data, dlc, tmp, 7);
            if (plen == 0)
                return false;
            reset();
            on_complete_(std::vector<uint8_t>(tmp, tmp + plen));
            return true;
        }

        case IsoTpFrameType::First: {
            uint8_t first_payload[6]{};
            uint8_t first_len = 0;
            uint16_t total = decode_first_frame(data, dlc, first_payload, first_len);
            if (total == 0)
                return false;
            reset();
            state_        = State::Receiving;
            expected_len_ = total;
            next_sn_      = 1;
            buf_.insert(buf_.end(), first_payload, first_payload + first_len);
            return true;
        }

        case IsoTpFrameType::Consecutive: {
            if (state_ != State::Receiving)
                return false;
            uint8_t sn = 0;
            uint8_t tmp[7]{};
            uint8_t plen = decode_consecutive_frame(data, dlc, sn, tmp);
            if (plen == 0)
                return false;
            if (sn != (next_sn_ & 0x0Fu)) {
                reset();  // sequence error — discard session
                return false;
            }
            next_sn_ = static_cast<uint8_t>((next_sn_ + 1u) & 0x0Fu);

            // Only append bytes up to expected_len_
            uint16_t remaining = static_cast<uint16_t>(expected_len_ - buf_.size());
            uint8_t  append    = plen < remaining ? plen : static_cast<uint8_t>(remaining);
            buf_.insert(buf_.end(), tmp, tmp + append);

            if (buf_.size() >= expected_len_) {
                on_complete_(std::move(buf_));
                reset();
            }
            return true;
        }

        case IsoTpFrameType::FlowControl:
            // Receiver does not process FC frames it sends them
            return false;
    }
    return false;
}

} // namespace socketspy::protocols::isotp
