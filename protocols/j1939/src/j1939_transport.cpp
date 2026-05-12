#include "j1939_transport.h"
#include <cstring>
#include <algorithm>

namespace socketspy::protocols::j1939 {

static constexpr uint32_t PGN_TP_CM = 0xEC00u;  // Transport Protocol Connection Management
static constexpr uint32_t PGN_TP_DT = 0xEB00u;  // Transport Protocol Data Transfer
static constexpr uint8_t  TP_CM_BAM = 0x20u;     // BAM control byte

J1939Transport::J1939Transport(PacketCallback on_complete)
    : on_complete_(std::move(on_complete)) {}

void J1939Transport::feed(uint32_t pgn, uint8_t sa, uint8_t da,
                           const uint8_t* data, uint8_t dlc,
                           uint64_t timestamp_us) {
    if (data == nullptr || dlc < 8)
        return;

    if (pgn == PGN_TP_CM && data[0] == TP_CM_BAM) {
        // BAM initiation: data[1..2]=total bytes LE, data[3]=total packets, data[5..7]=PGN
        Session s{};
        s.total_bytes   = static_cast<uint16_t>(data[1] | (data[2] << 8));
        s.total_packets = data[3];
        s.pgn = static_cast<uint32_t>(data[5])
              | (static_cast<uint32_t>(data[6]) << 8)
              | (static_cast<uint32_t>(data[7]) << 16);
        s.da  = da;
        s.received = 0;
        s.buf.assign(s.total_bytes, 0);
        s.last_activity_us = timestamp_us;
        sessions_[sa] = std::move(s);
        return;
    }

    if (pgn == PGN_TP_DT) {
        auto it = sessions_.find(sa);
        if (it == sessions_.end())
            return;

        Session& s = it->second;
        s.last_activity_us = timestamp_us;

        uint8_t seq = data[0];  // 1-based sequence number
        if (seq == 0 || seq > s.total_packets)
            return;

        // Write 7 payload bytes into the buffer at the right offset
        uint16_t offset = static_cast<uint16_t>((seq - 1u) * 7u);
        uint8_t  copy_len = 7u;
        if (static_cast<uint16_t>(offset + copy_len) > s.total_bytes)
            copy_len = static_cast<uint8_t>(s.total_bytes - offset);

        std::memcpy(s.buf.data() + offset, data + 1, copy_len);
        ++s.received;

        if (s.received >= s.total_packets) {
            J1939MultiPacket mp{};
            mp.pgn         = s.pgn;
            mp.sa          = sa;
            mp.da          = s.da;
            mp.total_bytes = s.total_bytes;
            mp.data        = std::move(s.buf);
            sessions_.erase(it);
            on_complete_(mp);
        }
    }
}

void J1939Transport::expire_stale(uint64_t now_us, uint64_t timeout_us) {
    for (auto it = sessions_.begin(); it != sessions_.end(); ) {
        if (now_us - it->second.last_activity_us > timeout_us)
            it = sessions_.erase(it);
        else
            ++it;
    }
}

} // namespace socketspy::protocols::j1939
