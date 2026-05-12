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

void CanCapture::run() {
    IfaceHandle h = can_open(m_iface.toStdString());
    if (!h.valid()) {
        emit errorOccurred(QString("can_open(%1) failed: %2")
            .arg(m_iface)
            .arg(strerror(errno)));
        return;
    }

    // Set 100 ms receive timeout so the loop can check m_stop
    struct timeval tv{0, 100'000};
    setsockopt(h.fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    uint64_t frame_count = 0;
    auto stat_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);

    while (!m_stop.load(std::memory_order_relaxed)) {
        can_frame raw{};
        ssize_t n = ::read(h.fd, &raw, sizeof(raw));

        auto now = std::chrono::steady_clock::now();

        if (n == static_cast<ssize_t>(sizeof(raw))) {
            CanFrame f{};
            // Monotonic timestamp in microseconds
            auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                          now.time_since_epoch()).count();
            f.timestamp_us = static_cast<uint64_t>(us);
            f.id           = raw.can_id & CAN_EFF_MASK;
            f.dlc          = raw.can_dlc;
            f.flags        = (raw.can_id & CAN_EFF_FLAG) ? 0 : 0;
            if (raw.can_id & CAN_EFF_FLAG)
                f.flags |= 0; // extended stored in id size
            f.iface_idx = h.idx;
            std::memcpy(f.data, raw.data, raw.can_dlc);
            ++frame_count;
            emit frameReceived(f);
        }
        // n < 0 on EAGAIN/timeout is expected — just loop

        if (now >= stat_deadline) {
            emit statsUpdated(frame_count);
            frame_count    = 0;
            stat_deadline  = now + std::chrono::seconds(1);
        }
    }

    can_close(h);
}

} // namespace socketspy::gui
