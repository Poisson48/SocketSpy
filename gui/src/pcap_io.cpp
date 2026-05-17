#include "pcap_io.h"
#include <QFile>
#include <cstring>
#include <cstdint>

// PCAP classic format — LINKTYPE_CAN_SOCKETCAN (227)
// Reference: https://wiki.wireshark.org/Development/LibpcapFileFormat

namespace socketspy::gui {

// ---------------------------------------------------------------------------
// Binary layout structs (packed, little-endian)
// ---------------------------------------------------------------------------

#pragma pack(push, 1)

// Global header — 24 bytes
struct PcapGlobalHdr {
    uint32_t magic_number;   // 0xa1b2c3d4
    uint16_t version_major;  // 2
    uint16_t version_minor;  // 4
    int32_t  thiszone;       // 0
    uint32_t sigfigs;        // 0
    uint32_t snaplen;        // 65535
    uint32_t network;        // 227 = LINKTYPE_CAN_SOCKETCAN
};
static_assert(sizeof(PcapGlobalHdr) == 24, "PcapGlobalHdr size mismatch");

// Per-packet record header — 16 bytes
struct PcapRecHdr {
    uint32_t ts_sec;
    uint32_t ts_usec;
    uint32_t incl_len;
    uint32_t orig_len;
};
static_assert(sizeof(PcapRecHdr) == 16, "PcapRecHdr size mismatch");

// SocketCAN frame as stored in PCAP payload — 16 bytes
struct PcapCanFrame {
    uint32_t can_id;   // bit31=EFF, bits28-0=ID
    uint8_t  can_dlc;
    uint8_t  pad[3];
    uint8_t  data[8];
};
static_assert(sizeof(PcapCanFrame) == 16, "PcapCanFrame size mismatch");

#pragma pack(pop)

static constexpr uint32_t kMagic        = 0xa1b2c3d4u;
static constexpr uint16_t kVerMajor     = 2;
static constexpr uint16_t kVerMinor     = 4;
static constexpr uint32_t kSnaplen      = 65535u;
static constexpr uint32_t kLinkTypeCan  = 227u;
static constexpr uint32_t kEFFFlag      = 0x80000000u; // extended frame flag

// ---------------------------------------------------------------------------
// PcapWriter
// ---------------------------------------------------------------------------

bool PcapWriter::write(const QString& path,
                       const std::vector<socketspy::core::CanFrame>& frames)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;

    // Write global header
    PcapGlobalHdr gh{};
    gh.magic_number  = kMagic;
    gh.version_major = kVerMajor;
    gh.version_minor = kVerMinor;
    gh.thiszone      = 0;
    gh.sigfigs       = 0;
    gh.snaplen       = kSnaplen;
    gh.network       = kLinkTypeCan;
    if (f.write(reinterpret_cast<const char*>(&gh), sizeof(gh)) != sizeof(gh))
        return false;

    for (const auto& frame : frames) {
        const uint32_t sec  = static_cast<uint32_t>(frame.timestamp_us / 1'000'000u);
        const uint32_t usec = static_cast<uint32_t>(frame.timestamp_us % 1'000'000u);

        // Build SocketCAN payload
        PcapCanFrame cf{};
        cf.can_id  = frame.id & 0x1FFFFFFFu;
        if (frame.id > 0x7FFu)
            cf.can_id |= kEFFFlag;  // extended frame
        cf.can_dlc = std::min(frame.dlc, static_cast<uint8_t>(8));
        std::memcpy(cf.data, frame.data, 8);

        PcapRecHdr rh{};
        rh.ts_sec  = sec;
        rh.ts_usec = usec;
        rh.incl_len = sizeof(PcapCanFrame);
        rh.orig_len = sizeof(PcapCanFrame);

        if (f.write(reinterpret_cast<const char*>(&rh), sizeof(rh)) != sizeof(rh))
            return false;
        if (f.write(reinterpret_cast<const char*>(&cf), sizeof(cf)) != sizeof(cf))
            return false;
    }

    f.close();
    return true;
}

// ---------------------------------------------------------------------------
// PcapReader
// ---------------------------------------------------------------------------

QList<socketspy::core::CanFrame> PcapReader::read(const QString& path)
{
    QList<socketspy::core::CanFrame> result;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return result;

    PcapGlobalHdr gh{};
    if (f.read(reinterpret_cast<char*>(&gh), sizeof(gh)) != sizeof(gh))
        return result;

    // Validate magic and link type
    if (gh.magic_number != kMagic)
        return result;
    if (gh.network != kLinkTypeCan)
        return result;

    while (!f.atEnd()) {
        PcapRecHdr rh{};
        if (f.read(reinterpret_cast<char*>(&rh), sizeof(rh)) != sizeof(rh))
            break;

        // Read payload (skip if not the expected SocketCAN frame size)
        const QByteArray payload = f.read(rh.incl_len);
        if (static_cast<uint32_t>(payload.size()) != rh.incl_len)
            break;
        if (rh.incl_len < sizeof(PcapCanFrame))
            continue;

        PcapCanFrame cf{};
        std::memcpy(&cf, payload.constData(), sizeof(PcapCanFrame));

        socketspy::core::CanFrame frame{};
        frame.timestamp_us = static_cast<uint64_t>(rh.ts_sec) * 1'000'000u
                           + static_cast<uint64_t>(rh.ts_usec);
        frame.id           = cf.can_id & 0x1FFFFFFFu;
        frame.dlc          = std::min(cf.can_dlc, static_cast<uint8_t>(8));
        std::memcpy(frame.data, cf.data, 8);
        result.append(frame);
    }

    return result;
}

} // namespace socketspy::gui
