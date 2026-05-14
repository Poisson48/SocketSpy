#include "can_simulator.h"
#include <chrono>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <random>

namespace socketspy::gui {

static uint64_t now_us() {
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count());
}

CanSimulator::CanSimulator(QObject* parent) : QObject(parent) {}

CanSimulator::~CanSimulator() { stop(); }

// ── Waveform generation ───────────────────────────────────────────────────────

std::vector<SimScenarioPoint> CanSimulator::generateWaveform(
        WaveformType waveform, double min_val, double max_val,
        int num_points, int step_ms)
{
    std::vector<SimScenarioPoint> pts;
    if (num_points <= 0 || step_ms <= 0) return pts;
    pts.reserve(static_cast<size_t>(num_points) + 1);

    const double range  = max_val - min_val;
    const double mid    = (min_val + max_val) / 2.0;
    const double amp    = range / 2.0;

    static std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<double> dist(min_val, max_val);

    for (int i = 0; i < num_points; ++i) {
        double phase = static_cast<double>(i) / static_cast<double>(num_points); // [0, 1)
        double value = 0.0;
        switch (waveform) {
        case WaveformType::Sine:
            value = mid + amp * std::sin(2.0 * M_PI * phase);
            break;
        case WaveformType::Ramp:
            value = min_val + range * phase;
            break;
        case WaveformType::Square:
            value = phase < 0.5 ? max_val : min_val;
            break;
        case WaveformType::Random:
            value = dist(rng);
            break;
        default:
            value = min_val;
            break;
        }
        SimScenarioPoint p;
        p.t_ms  = static_cast<int64_t>(i) * step_ms;
        p.value = std::clamp(value, min_val, max_val);
        pts.push_back(p);
    }

    // Add loop-back sentinel so interpolation wraps cleanly
    if (!pts.empty()) {
        SimScenarioPoint sentinel;
        sentinel.t_ms  = static_cast<int64_t>(num_points) * step_ms;
        sentinel.value = pts.front().value;
        pts.push_back(sentinel);
    }
    return pts;
}

// ─────────────────────────────────────────────────────────────────────────────

void CanSimulator::load(const SimProfile& profile) {
    stop();
    m_profile = profile;
    // Generate waveform scenarios for signals that have a waveform type set
    for (auto& node : m_profile.nodes)
        for (auto& msg : node.messages)
            for (auto& sig : msg.sigs) {
                if (sig.waveform != WaveformType::None) {
                    sig.scenario = generateWaveform(
                        sig.waveform, sig.min, sig.max, sig.num_points, sig.step_ms);
                }
                // Ensure scenario points are sorted by t_ms
                if (sig.scenario.size() > 1)
                    std::sort(sig.scenario.begin(), sig.scenario.end(),
                              [](const SimScenarioPoint& a, const SimScenarioPoint& b) {
                                  return a.t_ms < b.t_ms;
                              });
            }
}

double CanSimulator::interpolateScenario(const std::vector<SimScenarioPoint>& pts, int64_t t_ms) {
    if (pts.empty()) return 0.0;
    if (pts.size() == 1) return pts[0].value;
    int64_t loop_ms = pts.back().t_ms;
    if (loop_ms <= 0) return pts.back().value;
    t_ms = t_ms % loop_ms;
    for (size_t i = 1; i < pts.size(); ++i) {
        if (t_ms <= pts[i].t_ms) {
            double dt = static_cast<double>(pts[i].t_ms - pts[i - 1].t_ms);
            if (dt <= 0.0) return pts[i].value;
            double alpha = static_cast<double>(t_ms - pts[i - 1].t_ms) / dt;
            return pts[i - 1].value + alpha * (pts[i].value - pts[i - 1].value);
        }
    }
    return pts.back().value;
}

void CanSimulator::tickAnimations() {
    m_elapsed_ms += kAnimInterval;
    for (auto& node : m_profile.nodes)
        for (auto& msg : node.messages)
            for (auto& sig : msg.sigs)
                if (!sig.scenario.empty())
                    sig.current_value = interpolateScenario(sig.scenario, m_elapsed_ms);
    emit animationTick();
}

int64_t CanSimulator::scenarioDuration() const {
    int64_t max = 0;
    for (const auto& node : m_profile.nodes)
        for (const auto& msg : node.messages)
            for (const auto& sig : msg.sigs)
                if (!sig.scenario.empty() && sig.scenario.back().t_ms > max)
                    max = sig.scenario.back().t_ms;
    return max > 0 ? max : 1000;
}

void CanSimulator::resetElapsed() {
    m_elapsed_ms = 0;
}

double CanSimulator::signalValue(int ni, int mi, int si) const {
    if (ni < 0 || ni >= (int)m_profile.nodes.size()) return 0.0;
    const auto& node = m_profile.nodes[ni];
    if (mi < 0 || mi >= (int)node.messages.size()) return 0.0;
    const auto& msg = node.messages[mi];
    if (si < 0 || si >= (int)msg.sigs.size()) return 0.0;
    return msg.sigs[si].current_value;
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
    m_elapsed_ms = 0;

    // Animation timer: drives scenario interpolation for all signals
    auto* animT = new QTimer(this);
    animT->setInterval(kAnimInterval);
    connect(animT, &QTimer::timeout, this, &CanSimulator::tickAnimations);
    animT->start();
    m_timers.push_back(animT);

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

void CanSimulator::setSignalWaveform(int ni, int mi, int si,
                                     WaveformType waveform, double min_val, double max_val,
                                     int num_points, int step_ms)
{
    if (ni < 0 || ni >= (int)m_profile.nodes.size()) return;
    auto& node = m_profile.nodes[ni];
    if (mi < 0 || mi >= (int)node.messages.size()) return;
    auto& msg = node.messages[mi];
    if (si < 0 || si >= (int)msg.sigs.size()) return;
    auto& sig = msg.sigs[si];

    sig.waveform   = waveform;
    sig.min        = min_val;
    sig.max        = max_val;
    sig.num_points = num_points;
    sig.step_ms    = step_ms;

    if (waveform != WaveformType::None)
        sig.scenario = generateWaveform(waveform, min_val, max_val, num_points, step_ms);
    else
        sig.scenario.clear();
}

const SimSignal* CanSimulator::signalAt(int ni, int mi, int si) const {
    if (ni < 0 || ni >= (int)m_profile.nodes.size()) return nullptr;
    const auto& node = m_profile.nodes[ni];
    if (mi < 0 || mi >= (int)node.messages.size()) return nullptr;
    const auto& msg = node.messages[mi];
    if (si < 0 || si >= (int)msg.sigs.size()) return nullptr;
    return &msg.sigs[si];
}

} // namespace socketspy::gui
