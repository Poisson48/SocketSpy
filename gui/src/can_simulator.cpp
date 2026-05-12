#include "can_simulator.h"
#include <chrono>
#include <algorithm>
#include <cstring>

namespace socketspy::gui {

static uint64_t now_us() {
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count());
}

CanSimulator::CanSimulator(QObject* parent) : QObject(parent) {}

CanSimulator::~CanSimulator() { stop(); }

void CanSimulator::load(const SimProfile& profile) {
    stop();
    m_profile = profile;
}

void CanSimulator::encodeSignal(uint8_t* data, const SimSignal& sig) {
    double clamped = std::clamp(sig.current_value, sig.min, sig.max);
    int64_t raw = static_cast<int64_t>((clamped - sig.offset) / sig.factor);
    const int64_t max_raw = (1LL << sig.length) - 1;
    raw = std::clamp(raw, int64_t{0}, max_raw);

    for (int bit = 0; bit < sig.length; ++bit) {
        const int abs_bit = sig.start_bit + bit;
        const int byte_idx = abs_bit / 8;
        const int bit_idx  = abs_bit % 8;
        if ((raw >> bit) & 1)
            data[byte_idx] |= static_cast<uint8_t>(1u << bit_idx);
        else
            data[byte_idx] &= static_cast<uint8_t>(~(1u << bit_idx));
    }
}

void CanSimulator::encodeAndEmit(int node_idx, int msg_idx) {
    const SimMessage& msg = m_profile.nodes[node_idx].messages[msg_idx];
    socketspy::core::CanFrame frame{};
    frame.timestamp_us = now_us();
    frame.id           = msg.id;
    frame.dlc          = msg.dlc;
    frame.flags        = 0;
    frame.iface_idx    = kSimIfaceIdx;
    std::memset(frame.data, 0, sizeof(frame.data));
    for (const auto& sig : msg.sigs)
        encodeSignal(frame.data, sig);
    emit frameGenerated(frame);
}

void CanSimulator::start() {
    if (m_running) return;

    int total_msgs = 0;
    for (const auto& node : m_profile.nodes)
        total_msgs += static_cast<int>(node.messages.size());

    if (total_msgs > kMaxTimers) {
        auto* global = new QTimer(this);
        global->setInterval(10);
        std::vector<int64_t> next_fire;
        std::vector<std::pair<int,int>> msg_coords;
        for (int ni = 0; ni < (int)m_profile.nodes.size(); ++ni)
            for (int mi = 0; mi < (int)m_profile.nodes[ni].messages.size(); ++mi) {
                msg_coords.push_back({ni, mi});
                next_fire.push_back(0);
            }
        int64_t elapsed = 0;
        connect(global, &QTimer::timeout, this, [this, next_fire, msg_coords, elapsed]() mutable {
            elapsed += 10;
            for (int i = 0; i < (int)msg_coords.size(); ++i) {
                if (elapsed >= next_fire[i]) {
                    const int period = m_profile.nodes[msg_coords[i].first]
                                           .messages[msg_coords[i].second].period_ms;
                    next_fire[i] = elapsed + period;
                    encodeAndEmit(msg_coords[i].first, msg_coords[i].second);
                }
            }
        });
        global->start();
        m_timers.push_back(global);
    } else {
        for (int ni = 0; ni < (int)m_profile.nodes.size(); ++ni) {
            for (int mi = 0; mi < (int)m_profile.nodes[ni].messages.size(); ++mi) {
                const int period = m_profile.nodes[ni].messages[mi].period_ms;
                auto* t = new QTimer(this);
                t->setInterval(period);
                connect(t, &QTimer::timeout, this, [this, ni, mi]() {
                    encodeAndEmit(ni, mi);
                });
                t->start();
                m_timers.push_back(t);
            }
        }
    }
    m_running = true;
}

void CanSimulator::stop() {
    for (auto* t : m_timers) {
        t->stop();
        delete t;
    }
    m_timers.clear();
    m_running = false;
}

bool CanSimulator::isRunning() const { return m_running; }

void CanSimulator::setSignalValue(int node_idx, int msg_idx, int sig_idx, double value) {
    if (node_idx < 0 || node_idx >= (int)m_profile.nodes.size()) return;
    auto& node = m_profile.nodes[node_idx];
    if (msg_idx < 0 || msg_idx >= (int)node.messages.size()) return;
    auto& msg = node.messages[msg_idx];
    if (sig_idx < 0 || sig_idx >= (int)msg.sigs.size()) return;
    msg.sigs[sig_idx].current_value = value;
}

} // namespace socketspy::gui
