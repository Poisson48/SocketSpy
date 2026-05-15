#include "blf_writer.h"
#include <QFile>
#include <QDataStream>
#include <cstring>
#include <cstdint>
#include <chrono>

namespace socketspy::gui {

// ---- BLF constants ---------------------------------------------------------
// Object signatures
static constexpr uint32_t kSigBase  = 0x4C4F4742; // "LOBJ"
static constexpr uint32_t kSigFile  = 0x47424F4C; // "LOBG" — unused, kept for reference
static constexpr uint32_t kSigStats = 0x53544154; // "STAT"

// Object types
static constexpr uint16_t kObjTypeCanMsg          = 1;
static constexpr uint16_t kObjTypeLogContainerRaw = 10;
// We won't use log containers — write raw objects directly.

// Header size (base object header)
static constexpr uint32_t kBaseHdrSize = 16; // sig(4)+hdr_size(2)+hdr_ver(2)+obj_size(4)+obj_type(2)+timestamp(4)
// Actually the real BLF base header is:
//   signature      4 bytes  "LOBJ"
//   header_size    2 bytes  (= 0x10 = 16 for base)
//   header_version 2 bytes  (= 1)
//   object_size    4 bytes  total object size incl header
//   object_type    4 bytes
//   timestamp      8 bytes  (100ns units since 1980-01-01)
// Total base header = 24 bytes

static constexpr uint16_t kHdrSize    = 24;
static constexpr uint16_t kHdrVersion = 1;

// Compression method (none)
static constexpr uint8_t kCompNone = 0;

// Application ID for SocketSpy
static constexpr uint8_t kAppId = 0;

// FILETIME epoch difference between 1601 and 1980 in 100ns units
// (used to fill the Statistics block's start/end timestamps)
// We just use 0 for simplicity — CANalyzer accepts it.

#pragma pack(push, 1)
struct BlfFileSignature {
    char     signature[4];    // "BLF0"
    uint32_t api_version;     // 0x0403 = 4.3 (use 0x0200 = 2.0 to match "BLF0200")
    char     app_name[128];   // application name (null-padded)
    uint8_t  app_build_ver[4];
    uint8_t  app_ver[4];
};

// System time structure (matches Windows SYSTEMTIME)
struct BlfSystemTime {
    uint16_t year;
    uint16_t month;
    uint16_t dow;      // day of week
    uint16_t day;
    uint16_t hour;
    uint16_t minute;
    uint16_t second;
    uint16_t ms;
};

struct BlfStatistics {
    uint32_t object_count;
    uint32_t measured_days;
    uint32_t measured_ms;
    BlfSystemTime start_time;
    BlfSystemTime end_time;
};

// Full BLF file header (216 bytes)
struct BlfFileHeader {
    char          signature[4];   // "BLF0"
    uint32_t      statistics_size; // total size of header = 144
    uint16_t      api_version;    // 0x0403
    uint8_t       app_name[128];
    uint8_t       app_build_ver[4];
    uint8_t       app_ver[4];
    BlfSystemTime measure_start_time;
    BlfSystemTime last_object_time;
    uint32_t      object_count;
    uint32_t      reserved[18];
};
static_assert(sizeof(BlfFileHeader) == 4+4+2+128+4+4+16+16+4+18*4, "");

// Base object header
struct BlfBaseObjHdr {
    char     signature[4]; // "LOBJ"
    uint16_t header_size;  // = 24
    uint16_t header_ver;   // = 1
    uint32_t object_size;  // total bytes including this header
    uint32_t object_type;
    uint32_t timestamp_lo; // lo 32 bits of 100ns timestamp
    uint32_t timestamp_hi; // hi 32 bits
};
static_assert(sizeof(BlfBaseObjHdr) == 24, "");

// CAN message object body (after base header)
struct BlfCanMsgBody {
    uint16_t channel;   // 1-based channel number
    uint8_t  dlc;
    uint8_t  flags;     // bit0=TX
    uint32_t arb_id;
    uint8_t  data[8];
};
static_assert(sizeof(BlfCanMsgBody) == 16, "");

#pragma pack(pop)

static void writeLE16(QFile& f, uint16_t v) { f.write(reinterpret_cast<char*>(&v), 2); }
static void writeLE32(QFile& f, uint32_t v) { f.write(reinterpret_cast<char*>(&v), 4); }

// Convert microseconds since epoch to 100ns units (for BLF timestamps, relative to session start)
static uint64_t usTo100ns(uint64_t us) {
    return us * 10ULL;
}

bool BlfWriter::write(const QString& path,
                      const std::vector<socketspy::core::CanFrame>& frames)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;

    const uint32_t kStatSize = 144; // documented statistics block size

    // --- File Header ---
    // "BLF0200" marker as 4+2+1 doesn't exist as struct; we write "BLF0" then version 0x0200
    f.write("BLF0", 4);
    writeLE32(f, kStatSize);       // statistics_size
    writeLE16(f, 0x0403);          // api_version
    // app_name (128 bytes)
    {
        char name[128] = "SocketSpy";
        f.write(name, 128);
    }
    // app_build_ver (4 bytes) + app_ver (4 bytes)
    {
        uint8_t ver[8] = {1, 0, 0, 0, 1, 0, 0, 0};
        f.write(reinterpret_cast<char*>(ver), 8);
    }
    // measure_start_time (SYSTEMTIME, 16 bytes)
    {
        BlfSystemTime t{};
        t.year = 2024; t.month = 1; t.day = 1;
        f.write(reinterpret_cast<char*>(&t), 16);
    }
    // last_object_time (SYSTEMTIME, 16 bytes)
    {
        BlfSystemTime t{};
        t.year = 2024; t.month = 1; t.day = 1;
        f.write(reinterpret_cast<char*>(&t), 16);
    }
    // object_count
    writeLE32(f, static_cast<uint32_t>(frames.size()));
    // reserved (18 * 4 = 72 bytes)
    {
        uint8_t reserved[72] = {};
        f.write(reinterpret_cast<char*>(reserved), 72);
    }

    // --- CAN Message Objects ---
    for (const auto& frame : frames) {
        // Base object header
        f.write("LOBJ", 4);
        writeLE16(f, 24);    // header_size
        writeLE16(f, 1);     // header_ver
        // object_size = 24 (hdr) + 16 (body) = 40
        uint32_t objSize = 24 + 16;
        // FD frames have larger data but we cap at standard 8 for now in this minimal writer
        writeLE32(f, objSize);
        writeLE32(f, static_cast<uint32_t>(kObjTypeCanMsg));
        // timestamp in 100ns, relative to session start (use frame timestamp)
        uint64_t ts100ns = usTo100ns(frame.timestamp_us);
        writeLE32(f, static_cast<uint32_t>(ts100ns & 0xFFFFFFFFULL));
        writeLE32(f, static_cast<uint32_t>(ts100ns >> 32));
        // CAN message body
        writeLE16(f, 1);     // channel
        uint8_t dlc = std::min(frame.dlc, static_cast<uint8_t>(8));
        f.write(reinterpret_cast<const char*>(&dlc), 1);
        uint8_t flags = 0;
        f.write(reinterpret_cast<const char*>(&flags), 1);
        writeLE32(f, frame.id);
        f.write(reinterpret_cast<const char*>(frame.data), 8);
    }

    f.close();
    return true;
}

} // namespace socketspy::gui
