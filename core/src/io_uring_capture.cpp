#include "io_uring_capture.h"
#include "ring_buffer.h"
#include <liburing.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <net/if.h>
#include <unistd.h>
#include <atomic>
#include <thread>
#include <cstring>
#include <cerrno>
#include <cassert>

namespace socketspy::core {

// Number of pre-posted recvmsg operations in the submission queue.
// Must divide evenly into ring_depth; we keep it equal for simplicity.
static constexpr size_t kDefaultRingDepth = 64;

// Ancillary data buffer size for SO_TIMESTAMPING (3 * timespec = 72 bytes).
static constexpr size_t kCmsgBufSize = 128;

struct RecvContext {
    struct iovec      iov;
    struct msghdr     msg;
    struct canfd_frame frame;
    char              cmsg_buf[kCmsgBufSize];
};

struct IoUringCapture::Impl {
    Config                  cfg;
    RingBuffer&             dest;
    int                     fd{-1};
    struct io_uring         ring{};
    std::thread             thread;
    std::atomic<bool>       stop_flag{false};
    std::atomic<bool>       running_flag{false};
    std::atomic<uint64_t>   captured{0};
    std::atomic<uint64_t>   dropped{0};

    // Pool of RecvContext structs submitted as fixed SQEs.
    std::vector<RecvContext> ctxs;

    explicit Impl(const Config& c, RingBuffer& d) : cfg(c), dest(d) {}

    bool setup_socket() {
        fd = ::socket(PF_CAN, SOCK_RAW | SOCK_NONBLOCK, CAN_RAW);
        if (fd < 0) return false;

        can_err_mask_t err_mask = CAN_ERR_MASK;
        ::setsockopt(fd, SOL_CAN_RAW, CAN_RAW_ERR_FILTER,
                     &err_mask, sizeof(err_mask));

        if (cfg.fd_mode) {
            int one = 1;
            ::setsockopt(fd, SOL_CAN_RAW, CAN_RAW_FD_FRAMES,
                         &one, sizeof(one));
        }

        int ts_flags = SOF_TIMESTAMPING_RX_HARDWARE |
                       SOF_TIMESTAMPING_RX_SOFTWARE |
                       SOF_TIMESTAMPING_SOFTWARE;
        ::setsockopt(fd, SOL_SOCKET, SO_TIMESTAMPING,
                     &ts_flags, sizeof(ts_flags));

        char ifname[IFNAMSIZ] = {};
        cfg.iface.copy(ifname, sizeof(ifname) - 1);
        struct ifreq ifr{};
        std::strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
        if (::ioctl(fd, SIOCGIFINDEX, &ifr) < 0) {
            ::close(fd); fd = -1;
            return false;
        }

        struct sockaddr_can addr{};
        addr.can_family  = AF_CAN;
        addr.can_ifindex = ifr.ifr_ifindex;
        if (::bind(fd, reinterpret_cast<struct sockaddr*>(&addr),
                   sizeof(addr)) < 0) {
            ::close(fd); fd = -1;
            return false;
        }
        return true;
    }

    void init_ctx(RecvContext& ctx) {
        std::memset(&ctx, 0, sizeof(ctx));
        ctx.iov.iov_base = &ctx.frame;
        ctx.iov.iov_len  = sizeof(ctx.frame);
        ctx.msg.msg_iov        = &ctx.iov;
        ctx.msg.msg_iovlen     = 1;
        ctx.msg.msg_control    = ctx.cmsg_buf;
        ctx.msg.msg_controllen = kCmsgBufSize;
    }

    void submit_recv(RecvContext* ctx) {
        struct io_uring_sqe* sqe = io_uring_get_sqe(&ring);
        assert(sqe);
        io_uring_prep_recvmsg(sqe, fd, &ctx->msg, 0);
        io_uring_sqe_set_data(sqe, ctx);
    }

    void run() {
        running_flag.store(true, std::memory_order_release);

        const size_t depth = cfg.ring_depth;
        ctxs.resize(depth);
        for (auto& ctx : ctxs) {
            init_ctx(ctx);
            submit_recv(&ctx);
        }
        io_uring_submit(&ring);

        while (!stop_flag.load(std::memory_order_acquire)) {
            struct io_uring_cqe* cqe = nullptr;
            struct __kernel_timespec ts{0, 5'000'000};  // 5ms wait
            int ret = io_uring_wait_cqe_timeout(&ring, &cqe, &ts);

            if (ret == -ETIME || ret == -EINTR) continue;
            if (ret < 0) break;

            auto* ctx = static_cast<RecvContext*>(io_uring_cqe_get_data(cqe));
            io_uring_cqe_seen(&ring, cqe);

            if (cqe->res > 0) {
                CanFrame f{};
                const bool is_fd = (cqe->res == sizeof(struct canfd_frame));
                f.id        = ctx->frame.can_id & CAN_EFF_MASK;
                f.dlc       = ctx->frame.len;
                f.iface_idx = cfg.iface_idx;
                f.flags     = is_fd ? static_cast<uint8_t>(FrameFlags::FD) : 0;
                std::memcpy(f.data, ctx->frame.data,
                            std::min<uint8_t>(f.dlc, 64));
                f.timestamp_us = extract_timestamp_us(ctx->msg);
                if (f.timestamp_us == 0) {
                    struct timespec mono{};
                    ::clock_gettime(CLOCK_MONOTONIC, &mono);
                    f.timestamp_us = static_cast<uint64_t>(mono.tv_sec) * 1'000'000ULL
                                   + static_cast<uint64_t>(mono.tv_nsec) / 1'000ULL;
                }

                if (dest.push(f))
                    captured.fetch_add(1, std::memory_order_relaxed);
                else
                    dropped.fetch_add(1, std::memory_order_relaxed);
            }

            // Re-arm this slot.
            init_ctx(ctx);
            submit_recv(ctx);
            io_uring_submit(&ring);
        }

        running_flag.store(false, std::memory_order_release);
    }
};

IoUringCapture::IoUringCapture(const Config& cfg, RingBuffer& dest)
    : impl_(std::make_unique<Impl>(cfg, dest)) {}

IoUringCapture::~IoUringCapture() { stop(); }

bool IoUringCapture::start() {
    if (!impl_->setup_socket()) return false;

    const size_t depth = impl_->cfg.ring_depth > 0
                       ? impl_->cfg.ring_depth
                       : kDefaultRingDepth;
    impl_->cfg.ring_depth = depth;

    if (io_uring_queue_init(static_cast<unsigned>(depth),
                            &impl_->ring, 0) < 0)
        return false;

    impl_->thread = std::thread([this] { impl_->run(); });
    return true;
}

void IoUringCapture::stop() {
    if (!impl_->running_flag.load(std::memory_order_acquire)) return;
    impl_->stop_flag.store(true, std::memory_order_release);
    if (impl_->thread.joinable()) impl_->thread.join();
    io_uring_queue_exit(&impl_->ring);
    if (impl_->fd >= 0) { ::close(impl_->fd); impl_->fd = -1; }
}

bool IoUringCapture::running() const noexcept {
    return impl_->running_flag.load(std::memory_order_acquire);
}

uint64_t IoUringCapture::frames_captured() const noexcept {
    return impl_->captured.load(std::memory_order_relaxed);
}

uint64_t IoUringCapture::frames_dropped() const noexcept {
    return impl_->dropped.load(std::memory_order_relaxed);
}

} // namespace socketspy::core
