#include "can_capture.h"
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <cerrno>
#include <cstring>
#include <chrono>

using namespace socketspy::core;

namespace socketspy::gui {

CanCapture::CanCapture(QString iface, QObject* parent)
    : QThread(parent), m_iface(std::move(iface))
{}

CanCapture::~CanCapture() {
    stop();
    wait();
}

void CanCapture::stop() {
    m_stop.store(true, std::memory_order_relaxed);
}

void CanCapture::setTrigger(TriggerConfig cfg) {
    std::lock_guard<std::mutex> lk(m_triggerMutex);
    m_trigger = cfg;
    m_triggerArmed.store(cfg.enabled, std::memory_order_relaxed);
}

void CanCapture::run() {
    IfaceHandle h = can_open(m_iface.toStdString());
    if (!h.valid()) {
        emit errorOccurred(QString("can_open(%1) failed: %2")
            .arg(m_iface)
            .arg(strerror(errno)));
        return;
    }

    // Enable CAN FD frames on this socket (falls back gracefully on non-FD buses)
    can_set_fd_mode(h, true);

    // Set 100 ms receive timeout so the loop can check m_stop
    struct timeval tv{0, 100'000};
    setsockopt(h.fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    uint64_t frame_count = 0;
    double   smooth_fps  = 0.0;
    auto stat_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);

    while (!m_stop.load(std::memory_order_relaxed)) {
        // Use canfd_frame as buffer — large enough for both classic (8 B) and FD (64 B)
        canfd_frame raw{};
        ssize_t n = ::read(h.fd, &raw, sizeof(raw));

        auto now = std::chrono::steady_clock::now();

        if (n == static_cast<ssize_t>(sizeof(can_frame)) ||
            n == static_cast<ssize_t>(sizeof(canfd_frame))) {

            const bool is_fd = (n == static_cast<ssize_t>(sizeof(canfd_frame)));

            CanFrame f{};
            auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                          now.time_since_epoch()).count();
            f.timestamp_us = static_cast<uint64_t>(us);
            f.id           = raw.can_id & CAN_EFF_MASK;
            f.dlc          = raw.len;   // canfd_frame uses .len (== can_dlc for classic)
            f.iface_idx    = h.idx;

            if (is_fd) {
                f.flags = static_cast<uint8_t>(FrameFlags::FD);
                if (raw.flags & CANFD_BRS)
                    f.flags |= static_cast<uint8_t>(FrameFlags::BRS);
                if (raw.flags & CANFD_ESI)
                    f.flags |= static_cast<uint8_t>(FrameFlags::ESI);
            } else {
                f.flags = 0;
            }

            std::memcpy(f.data, raw.data, raw.len);
            ++frame_count;
            emit frameReceived(f);

            if (m_triggerArmed.load(std::memory_order_relaxed)) {
                TriggerConfig snap;
                {
                    std::lock_guard<std::mutex> lk(m_triggerMutex);
                    snap = m_trigger;
                }
                if (snap.match(f)) {
                    m_triggerArmed.store(false, std::memory_order_relaxed);
                    emit triggerFired();
                }
            }
        }

        if (now >= stat_deadline) {
            // Exponential moving average: α=0.3 gives ~3-sample smoothing
            constexpr double kAlpha = 0.3;
            smooth_fps   = kAlpha * static_cast<double>(frame_count) + (1.0 - kAlpha) * smooth_fps;
            emit statsUpdated(static_cast<uint64_t>(smooth_fps + 0.5));
            frame_count  = 0;
            stat_deadline = now + std::chrono::seconds(1);
        }
    }

    can_close(h);
}

} // namespace socketspy::gui
