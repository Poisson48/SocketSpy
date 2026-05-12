#pragma once
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

namespace socketspy::protocols::j1939 {

// Reassembled multi-packet message
struct J1939MultiPacket {
    uint32_t pgn;
    uint8_t  sa;
    uint8_t  da;
    uint16_t total_bytes;
    std::vector<uint8_t> data;
};

using PacketCallback = std::function<void(const J1939MultiPacket&)>;

// Reassembles TP.BAM (PGN 0xECFF) and TP.DT (PGN 0xEB00) streams.
// Call feed() for every J1939 frame on the bus.
class J1939Transport {
public:
    explicit J1939Transport(PacketCallback on_complete);

    void feed(uint32_t pgn, uint8_t sa, uint8_t da,
              const uint8_t* data, uint8_t dlc,
              uint64_t timestamp_us);

    // Clear stale sessions older than timeout_us.
    void expire_stale(uint64_t now_us, uint64_t timeout_us = 5'000'000);

private:
    struct Session {
        uint32_t pgn;
        uint8_t  da;
        uint16_t total_bytes;
        uint8_t  total_packets;
        uint8_t  received;
        std::vector<uint8_t> buf;
        uint64_t last_activity_us;
    };
    std::unordered_map<uint8_t, Session> sessions_;  // keyed by sa
    PacketCallback on_complete_;
};

} // namespace socketspy::protocols::j1939
