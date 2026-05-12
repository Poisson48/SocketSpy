#pragma once
#include <cstdint>
#include <functional>
#include <span>
#include <string_view>

// Forward-declare msghdr so callers need not pull in <sys/socket.h> just for
// extract_timestamp_us. Callers that *use* the function must include it themselves.
struct msghdr;

namespace socketspy::core {

// ---------------------------------------------------------------------------
// CAN frame flags
// ---------------------------------------------------------------------------

enum class FrameFlags : uint8_t {
    None  = 0,
    FD    = 1 << 0,  // CAN FD frame
    BRS   = 1 << 1,  // Bit Rate Switch
    ESI   = 1 << 2,  // Error State Indicator
    RTR   = 1 << 3,  // Remote Transmission Request
    Error = 1 << 4,  // Error frame
};

// ---------------------------------------------------------------------------
// Timestamped CAN frame — sized to fit in two cache lines (aligned to 64 B)
// ---------------------------------------------------------------------------

struct alignas(64) CanFrame {
    uint64_t timestamp_us;  // hardware or monotonic, microseconds
    uint32_t id;            // CAN ID (11 or 29 bit)
    uint8_t  dlc;           // data length code (0-15 for FD)
    uint8_t  flags;         // FrameFlags bitmask
    uint8_t  iface_idx;     // interface index (0-based)
    uint8_t  _pad;
    uint8_t  data[64];      // FD max; standard frames use [0:7]
};
static_assert(sizeof(CanFrame) <= 128, "CanFrame too large");

// ---------------------------------------------------------------------------
// Lock-free SPSC ring buffer
// Producer: capture thread only.  Consumer: decode thread only.
// ---------------------------------------------------------------------------

class RingBuffer {
public:
    explicit RingBuffer(size_t capacity);  // capacity must be power of 2
    ~RingBuffer();

    // Producer side (capture thread only)
    bool push(const CanFrame& frame) noexcept;

    // Consumer side (decode thread only)
    bool pop(CanFrame& frame) noexcept;

    // Approximate stats — not synchronised between threads
    size_t size()     const noexcept;
    size_t capacity() const noexcept;
    bool   empty()    const noexcept;
    bool   full()     const noexcept;

    RingBuffer(const RingBuffer&)            = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;

private:
    void* impl_;  // owning pointer to RingBufferImpl (pimpl)
};

// ---------------------------------------------------------------------------
// Interface handle
// ---------------------------------------------------------------------------

struct IfaceHandle {
    int     fd;   // raw SocketCAN file descriptor
    uint8_t idx;  // index in the interface manager
    bool valid() const noexcept { return fd >= 0; }
};
inline constexpr IfaceHandle kInvalidHandle{-1, 0};

// ---------------------------------------------------------------------------
// Per-frame callback type
// ---------------------------------------------------------------------------

using FrameCallback = std::function<void(const CanFrame&)>;

// ---------------------------------------------------------------------------
// SocketCAN interface operations
// ---------------------------------------------------------------------------

// Open a SocketCAN interface.  Returns kInvalidHandle on failure.
IfaceHandle can_open(std::string_view iface_name);

// Close a previously opened interface.
void can_close(IfaceHandle handle);

// Send a single frame synchronously.  Returns true on success.
bool can_send(IfaceHandle handle, const CanFrame& frame);

// Set kernel-level ID+mask filters (up to 16).
// Passing an empty span clears all filters (pass-all).
struct CanFilter { uint32_t id; uint32_t mask; };
bool can_set_filters(IfaceHandle handle, std::span<const CanFilter> filters);

// Enable/disable CAN FD mode on the socket.
bool can_set_fd_mode(IfaceHandle handle, bool enable);

// ---------------------------------------------------------------------------
// Error classification
// ---------------------------------------------------------------------------

enum class ErrorType : uint8_t {
    None = 0,
    BitError,
    StuffError,
    FormError,
    AckError,
    CrcError,
    BusOff,
    BusError,
};
ErrorType classify_error(const CanFrame& frame) noexcept;

// ---------------------------------------------------------------------------
// Timestamp extraction
// ---------------------------------------------------------------------------

// Decode hardware or software timestamp from recvmsg ancdata.
// Returns microseconds since CLOCK_MONOTONIC epoch.
uint64_t extract_timestamp_us(const struct msghdr& msg) noexcept;

} // namespace socketspy::core
