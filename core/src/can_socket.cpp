#include "cancore.h"
#include <linux/can.h>
#include <linux/can/raw.h>
#include <linux/can/error.h>
#include <net/if.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>

namespace socketspy::core {

IfaceHandle can_open(std::string_view iface_name) {
    int fd = ::socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (fd < 0) return kInvalidHandle;

    // Enable error frames so classify_error works.
    can_err_mask_t err_mask = CAN_ERR_MASK;
    if (::setsockopt(fd, SOL_CAN_RAW, CAN_RAW_ERR_FILTER,
                     &err_mask, sizeof(err_mask)) < 0) {
        ::close(fd);
        return kInvalidHandle;
    }

    // Request hardware timestamps with software fallback.
    int ts_flags = SOF_TIMESTAMPING_RX_HARDWARE |
                   SOF_TIMESTAMPING_RX_SOFTWARE |
                   SOF_TIMESTAMPING_SOFTWARE;
    if (::setsockopt(fd, SOL_SOCKET, SO_TIMESTAMPING,
                     &ts_flags, sizeof(ts_flags)) < 0) {
        // Timestamp support is optional; do not fail.
    }

    char ifname[IFNAMSIZ] = {};
    iface_name.copy(ifname, sizeof(ifname) - 1);

    struct ifreq ifr {};
    std::strncpy(ifr.ifr_name, ifname, IFNAMSIZ - 1);
    if (::ioctl(fd, SIOCGIFINDEX, &ifr) < 0) {
        ::close(fd);
        return kInvalidHandle;
    }

    struct sockaddr_can addr {};
    addr.can_family  = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (::bind(fd, reinterpret_cast<struct sockaddr*>(&addr),
               sizeof(addr)) < 0) {
        ::close(fd);
        return kInvalidHandle;
    }

    return IfaceHandle{fd, 0};
}

void can_close(IfaceHandle handle) {
    if (handle.valid()) ::close(handle.fd);
}

bool can_send(IfaceHandle handle, const CanFrame& frame) {
    if (!handle.valid()) return false;

    const bool is_fd = (frame.flags & static_cast<uint8_t>(FrameFlags::FD)) != 0;

    if (is_fd) {
        struct canfd_frame cf {};
        cf.can_id  = frame.id;
        cf.len     = frame.dlc;
        cf.flags   = 0;
        if (frame.flags & static_cast<uint8_t>(FrameFlags::BRS)) cf.flags |= CANFD_BRS;
        if (frame.flags & static_cast<uint8_t>(FrameFlags::ESI)) cf.flags |= CANFD_ESI;
        std::memcpy(cf.data, frame.data,
                    std::min<uint8_t>(frame.dlc, CANFD_MAX_DLEN));
        return ::write(handle.fd, &cf, sizeof(cf)) == sizeof(cf);
    } else {
        struct can_frame cf {};
        cf.can_id  = frame.id;
        if (frame.flags & static_cast<uint8_t>(FrameFlags::RTR))
            cf.can_id |= CAN_RTR_FLAG;
        cf.can_dlc = frame.dlc;
        std::memcpy(cf.data, frame.data,
                    std::min<uint8_t>(frame.dlc, CAN_MAX_DLEN));
        return ::write(handle.fd, &cf, sizeof(cf)) == sizeof(cf);
    }
}

bool can_set_filters(IfaceHandle handle, std::span<const CanFilter> filters) {
    if (!handle.valid()) return false;

    if (filters.empty()) {
        // Pass-all: use CAN_RAW_FILTER with zero filters.
        return ::setsockopt(handle.fd, SOL_CAN_RAW, CAN_RAW_FILTER,
                            nullptr, 0) == 0;
    }

    // The kernel accepts at most CAN_RAW_FILTER_MAX (512) filters.
    static_assert(sizeof(CanFilter) == sizeof(struct can_filter));
    return ::setsockopt(handle.fd, SOL_CAN_RAW, CAN_RAW_FILTER,
                        filters.data(),
                        static_cast<socklen_t>(filters.size_bytes())) == 0;
}

bool can_set_fd_mode(IfaceHandle handle, bool enable) {
    if (!handle.valid()) return false;
    int val = enable ? 1 : 0;
    return ::setsockopt(handle.fd, SOL_CAN_RAW, CAN_RAW_FD_FRAMES,
                        &val, sizeof(val)) == 0;
}

ErrorType classify_error(const CanFrame& frame) noexcept {
    if (!(frame.flags & static_cast<uint8_t>(FrameFlags::Error)))
        return ErrorType::None;

    const uint32_t eid = frame.id & CAN_ERR_MASK;

    if (eid & CAN_ERR_BUSOFF)     return ErrorType::BusOff;
    if (eid & CAN_ERR_BUSERROR)   return ErrorType::BusError;

    // Protocol error in data[2] (type) and data[3] (location)
    if (eid & CAN_ERR_PROT) {
        const uint8_t ptype = frame.data[2];
        if (ptype & CAN_ERR_PROT_BIT)   return ErrorType::BitError;
        if (ptype & CAN_ERR_PROT_STUFF) return ErrorType::StuffError;
        if (ptype & CAN_ERR_PROT_FORM)  return ErrorType::FormError;
        if (ptype & CAN_ERR_PROT_BIT0)  return ErrorType::BitError;
        if (ptype & CAN_ERR_PROT_BIT1)  return ErrorType::BitError;
    }

    if (eid & CAN_ERR_ACK)        return ErrorType::AckError;
    if (eid & CAN_ERR_CRTL) {
        // Controller: not a frame-level error type — report as generic
        return ErrorType::BusError;
    }

    return ErrorType::BusError;
}

uint64_t extract_timestamp_us(const struct msghdr& msg) noexcept {
    // Walk cmsg chain looking for SO_TIMESTAMPING.
    for (const struct cmsghdr* cm = CMSG_FIRSTHDR(&msg);
         cm != nullptr;
         cm = CMSG_NXTHDR(const_cast<struct msghdr*>(&msg), cm)) {

        if (cm->cmsg_level != SOL_SOCKET) continue;
        if (cm->cmsg_type  != SO_TIMESTAMPING) continue;

        // Three timespec values: software, hardware transformed, hardware raw.
        // Prefer hardware raw (index 2), fall back to software (index 0).
        const struct timespec* ts =
            reinterpret_cast<const struct timespec*>(CMSG_DATA(cm));
        const struct timespec& hw = ts[2];
        if (hw.tv_sec != 0 || hw.tv_nsec != 0) {
            return static_cast<uint64_t>(hw.tv_sec) * 1'000'000ULL +
                   static_cast<uint64_t>(hw.tv_nsec) / 1'000ULL;
        }
        const struct timespec& sw = ts[0];
        return static_cast<uint64_t>(sw.tv_sec) * 1'000'000ULL +
               static_cast<uint64_t>(sw.tv_nsec) / 1'000ULL;
    }

    // No timestamp ancdata — return 0 (caller will substitute monotonic clock).
    return 0;
}

} // namespace socketspy::core
