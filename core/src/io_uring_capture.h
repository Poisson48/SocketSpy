#pragma once
#include "cancore.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>

namespace socketspy::core {

// io_uring–based CAN frame capture.
//
// One IoUringCapture instance per interface, running on its own thread.
// Frames are pushed into a caller-supplied RingBuffer.
// The thread is started by start() and stops cleanly on stop().
class IoUringCapture {
public:
    struct Config {
        std::string_view iface;        // e.g. "can0", "vcan0"
        uint8_t          iface_idx;    // stored in CanFrame::iface_idx
        size_t           ring_depth;   // io_uring submission queue depth
        bool             fd_mode;      // enable CAN FD frames
    };

    explicit IoUringCapture(const Config& cfg, RingBuffer& dest);
    ~IoUringCapture();

    // Start the capture thread. Returns false if the interface cannot be opened.
    bool start();

    // Signal the capture thread to stop and block until it exits.
    void stop();

    // True if the capture thread is running.
    bool running() const noexcept;

    // Frames captured since start() (approximate, relaxed).
    uint64_t frames_captured() const noexcept;

    // Frames dropped because dest ring was full (approximate, relaxed).
    uint64_t frames_dropped() const noexcept;

    IoUringCapture(const IoUringCapture&)            = delete;
    IoUringCapture& operator=(const IoUringCapture&) = delete;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace socketspy::core
