#pragma once
#include <cstdint>
#include <functional>
#include <unordered_map>

namespace socketspy::protocols::canopen {
enum class NmtState : uint8_t;  // forward declaration

// Tracks heartbeat state for all nodes on a bus.
// Call update() for each received heartbeat frame.
class HeartbeatMonitor {
public:
    using MissedCallback = std::function<void(uint8_t node_id)>;

    explicit HeartbeatMonitor(uint32_t timeout_ms = 5000);

    // Call with decoded node_id and state from decode_heartbeat().
    void update(uint8_t node_id, NmtState state, uint64_t timestamp_us);

    // Call periodically (e.g. every 100ms) to detect missed heartbeats.
    void tick(uint64_t now_us, const MissedCallback& on_missed);

    // Returns last known state for node_id, or NmtState::Unknown.
    NmtState state_of(uint8_t node_id) const noexcept;

    // Returns number of nodes seen.
    size_t node_count() const noexcept;

private:
    struct NodeEntry {
        NmtState last_state;
        uint64_t last_seen_us;
    };
    uint32_t timeout_ms_;
    std::unordered_map<uint8_t, NodeEntry> nodes_;
};

} // namespace socketspy::protocols::canopen
