#include "canopen_heartbeat.h"
#include "canopen_nmt.h"

namespace socketspy::protocols::canopen {

HeartbeatMonitor::HeartbeatMonitor(uint32_t timeout_ms)
    : timeout_ms_(timeout_ms) {}

void HeartbeatMonitor::update(uint8_t node_id, NmtState state,
                               uint64_t timestamp_us) {
    nodes_[node_id] = NodeEntry{state, timestamp_us};
}

void HeartbeatMonitor::tick(uint64_t now_us, const MissedCallback& on_missed) {
    const uint64_t timeout_us = static_cast<uint64_t>(timeout_ms_) * 1000u;
    for (const auto& [node_id, entry] : nodes_) {
        if (now_us - entry.last_seen_us > timeout_us)
            on_missed(node_id);
    }
}

NmtState HeartbeatMonitor::state_of(uint8_t node_id) const noexcept {
    auto it = nodes_.find(node_id);
    if (it == nodes_.end())
        return NmtState::Unknown;
    return it->second.last_state;
}

size_t HeartbeatMonitor::node_count() const noexcept {
    return nodes_.size();
}

} // namespace socketspy::protocols::canopen
