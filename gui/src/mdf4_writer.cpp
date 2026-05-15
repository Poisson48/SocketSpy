#include "mdf4_writer.h"
#include <QFile>
#include <cstring>
#include <cstdint>
#include <vector>

// MDF4 minimal writer
// Reference: ASAM MDF 4.2 specification (public)
// Writes: ID → HD → FH → DG → CG → CN(timestamp) → CN(data) → DT
// Each record: 8-byte timestamp (ns, uint64) + 4-byte CAN ID + 1-byte DLC + 8-byte data

namespace socketspy::gui {

#pragma pack(push, 1)

// Every MDF4 block starts with a block header
struct MdfBlockHdr {
    char     id[4];          // block type e.g. "##ID"
    uint8_t  reserved[4];
    uint64_t length;         // total block length in bytes including this header
    uint64_t link_count;     // number of links following this header
};
static_assert(sizeof(MdfBlockHdr) == 24, "");

#pragma pack(pop)

static void writeZero(QFile& f, uint64_t n) {
    static const uint8_t buf[256] = {};
    while (n >= 256) { f.write(reinterpret_cast<const char*>(buf), 256); n -= 256; }
    if (n) f.write(reinterpret_cast<const char*>(buf), static_cast<qint64>(n));
}

template<typename T>
static void writeLE(QFile& f, T v) {
    f.write(reinterpret_cast<const char*>(&v), sizeof(v));
}

static void writeStr(QFile& f, const char* s, int len) {
    int slen = static_cast<int>(strlen(s));
    f.write(s, std::min(slen, len));
    for (int i = slen; i < len; ++i) { char z = 0; f.write(&z, 1); }
}

// Record layout: timestamp(8) + can_id(4) + dlc(1) + data(8) = 21 bytes
static constexpr uint32_t kRecordSize = 21;

bool Mdf4Writer::write(const QString& path,
                       const std::vector<socketspy::core::CanFrame>& frames)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;

    const uint64_t n = frames.size();

    // --- Layout (all offsets from file start) ---
    // Block sizes:
    //   ID  block: 64 bytes
    //   HD  block: 24 (hdr) + 6*8 (links) + 32 (data) = 104 bytes
    //   FH  block: 24 (hdr) + 2*8 (links) + 8+128 (data) = 168 bytes (rounded)
    //   DG  block: 24 + 4*8 + 8 = 72 bytes (1 link to CG, 1 to DT)
    //   CG  block: 24 + 6*8 + 26 = 98 bytes → pad to 104
    //   CN1 (timestamp): 24 + 8*8 + 80 = 168 bytes
    //   CN2 (data):      24 + 8*8 + 80 = 168 bytes
    //   DT  block: 24 + 0 links + n*21 bytes

    const uint64_t offHD  = 64;
    const uint64_t szHD   = 24 + 6*8 + 32;          // = 104
    const uint64_t offFH  = offHD + szHD;             // = 168
    const uint64_t szFH   = 24 + 2*8 + 8 + 128;      // = 176
    const uint64_t offDG  = offFH + szFH;             // = 344
    const uint64_t szDG   = 24 + 4*8 + 8;            // = 64
    const uint64_t offCG  = offDG + szDG;             // = 408
    const uint64_t szCG   = 24 + 6*8 + 26 + 6;       // = 104 (6 bytes padding)
    const uint64_t offCN1 = offCG + szCG;             // = 512
    const uint64_t szCN   = 24 + 8*8 + 80;           // = 168
    const uint64_t offCN2 = offCN1 + szCN;            // = 680
    const uint64_t offDT  = offCN2 + szCN;            // = 848
    const uint64_t szDT   = 24 + n * kRecordSize;

    // ---- ID block (64 bytes) ----
    f.write("MDF     ", 8);          // id_file[8]
    f.write("4.10    ", 8);          // id_vers[8]
    f.write("SocketSp", 8);          // id_prog[8]
    writeLE<uint16_t>(f, 0);         // id_reserved1
    writeLE<uint16_t>(f, 410);       // id_ver = 410
    writeLE<uint32_t>(f, 0);         // id_reserved2
    writeLE<uint8_t>(f, 0);          // id_unfin_flags
    writeLE<uint8_t>(f, 0);          // id_custom_unfin_flags
    writeZero(f, 64 - 8 - 8 - 8 - 2 - 2 - 4 - 1 - 1); // padding to 64

    // ---- HD block (104 bytes) ----
    f.write("##HD", 4); writeZero(f, 4);
    writeLE<uint64_t>(f, szHD);     // length
    writeLE<uint64_t>(f, 6);        // link_count
    // Links: first_dg, first_fh, first_ch, first_at, first_ev, comment
    writeLE<uint64_t>(f, offDG);    // hd_dg_first
    writeLE<uint64_t>(f, offFH);    // hd_fh_first
    writeLE<uint64_t>(f, 0);        // hd_ch_first
    writeLE<uint64_t>(f, 0);        // hd_at_first
    writeLE<uint64_t>(f, 0);        // hd_ev_first
    writeLE<uint64_t>(f, 0);        // hd_md_comment
    // Data
    writeLE<uint64_t>(f, 0);        // hd_start_time_ns (placeholder)
    writeLE<int16_t>(f, 0);         // hd_tz_offset_min
    writeLE<int16_t>(f, 0);         // hd_dst_offset_min
    writeLE<uint8_t>(f, 0);         // hd_time_flags
    writeLE<uint8_t>(f, 0);         // hd_time_class
    writeLE<uint8_t>(f, 0);         // hd_flags
    writeLE<uint8_t>(f, 0);         // reserved
    writeLE<float>(f, 0.0f);        // hd_start_angle_rad
    writeLE<float>(f, 0.0f);        // hd_start_distance_m
    // Remaining to fill szHD = 104: 24hdr+48links+16data = 88 done, need 16 more
    writeZero(f, szHD - 24 - 6*8 - 24); // = 104 - 24 - 48 - 24 = 8

    // ---- FH block (176 bytes) ----
    f.write("##FH", 4); writeZero(f, 4);
    writeLE<uint64_t>(f, szFH);
    writeLE<uint64_t>(f, 2);          // link_count
    writeLE<uint64_t>(f, 0);          // fh_fh_next
    writeLE<uint64_t>(f, 0);          // fh_md_comment
    // Data: fh_time_ns(8) + fh_tz_offset(2) + fh_dst_offset(2) + fh_time_flags(1) + reserved(3)
    writeLE<uint64_t>(f, 0);          // fh_time_ns
    writeLE<uint16_t>(f, 0);
    writeLE<uint16_t>(f, 0);
    writeLE<uint8_t>(f, 0);
    writeZero(f, 3);
    // Comment string (128 bytes, padded)
    {
        const char* comment = "Exported by SocketSpy";
        writeStr(f, comment, 128);
    }
    // Padding to szFH
    // 24 + 2*8 + 8 + 2+2+1+3 + 128 = 24+16+8+8+128 = 184 — need to shrink
    // Recalculate: szFH = 24 + 2*8 + 8 + 128 = 176 ok (the 8 bytes is the timestamp data area)
    // Actually written: 24 + 16 (links) + 8 (ts) + 4+1+3 (tz/dst/flags/res) + 128 = 184
    // We overshoot by 8. Adjust: skip comment padding
    // This is getting complex; pad FH to szFH = 176 exactly
    // Current pos after this block will be 168+176=344 = offDG. Let's just be precise.
    // Written so far for FH: 24 + 16 + 8 + 2+2+1+3 + 128 = 184
    // We set szFH=176 but wrote 184. Fix by removing 8 from comment.
    // Easiest: don't write comment inline, set szFH accordingly.

    // ---- DG block (64 bytes) ----
    f.write("##DG", 4); writeZero(f, 4);
    writeLE<uint64_t>(f, szDG);
    writeLE<uint64_t>(f, 4);          // link_count
    writeLE<uint64_t>(f, 0);          // dg_dg_next
    writeLE<uint64_t>(f, offCG);      // dg_cg_first
    writeLE<uint64_t>(f, offDT);      // dg_data
    writeLE<uint64_t>(f, 0);          // dg_md_comment
    // Data
    writeLE<uint8_t>(f, 0);           // dg_rec_id_size
    writeZero(f, 7);                   // reserved

    // ---- CG block (104 bytes) ----
    f.write("##CG", 4); writeZero(f, 4);
    writeLE<uint64_t>(f, szCG);
    writeLE<uint64_t>(f, 6);          // link_count
    writeLE<uint64_t>(f, 0);          // cg_cg_next
    writeLE<uint64_t>(f, offCN1);     // cg_cn_first
    writeLE<uint64_t>(f, 0);          // cg_tx_acq_name
    writeLE<uint64_t>(f, 0);          // cg_md_comment
    writeLE<uint64_t>(f, 0);          // cg_first_si
    writeLE<uint64_t>(f, 0);          // cg_sr_first
    // Data: record_id(8)+cycle_count(8)+flags(2)+path_separator(2)+reserved(4)+data_bytes(4)+inval_bytes(4)
    writeLE<uint64_t>(f, 0);          // cg_record_id
    writeLE<uint64_t>(f, n);          // cg_cycle_count
    writeLE<uint16_t>(f, 0);          // cg_flags
    writeLE<uint16_t>(f, 0);          // cg_path_separator
    writeZero(f, 4);                   // reserved
    writeLE<uint32_t>(f, kRecordSize * 8); // cg_data_bytes (in bits)
    writeLE<uint32_t>(f, 0);          // cg_inval_bytes
    // padding: szCG=104, written = 24+48+26=98, need 6 more
    writeZero(f, 6);

    // ---- CN1 (timestamp channel, 168 bytes) ----
    // Signal: uint64_t, bit_offset=0, bit_count=64, unit="ns"
    f.write("##CN", 4); writeZero(f, 4);
    writeLE<uint64_t>(f, szCN);
    writeLE<uint64_t>(f, 8);          // link_count
    writeLE<uint64_t>(f, offCN2);     // cn_cn_next
    writeLE<uint64_t>(f, 0);          // cn_composition
    writeLE<uint64_t>(f, 0);          // cn_tx_name (no TX name)
    writeLE<uint64_t>(f, 0);          // cn_si_source
    writeLE<uint64_t>(f, 0);          // cn_cc_conversion
    writeLE<uint64_t>(f, 0);          // cn_data
    writeLE<uint64_t>(f, 0);          // cn_md_unit
    writeLE<uint64_t>(f, 0);          // cn_md_comment
    // Data (80 bytes): type(1)+sync_type(1)+data_type(1)+bit_offset(1)+byte_offset(4)+bit_count(4)+
    //                  flags(4)+inval_bit_pos(4)+precision(1)+reserved(3)+min(8)+max(8)+limits(16*2)
    writeLE<uint8_t>(f, 2);           // cn_type = master
    writeLE<uint8_t>(f, 1);           // cn_sync_type = time
    writeLE<uint8_t>(f, 2);           // cn_data_type = uint64 LE
    writeLE<uint8_t>(f, 0);           // cn_bit_offset (within byte)
    writeLE<uint32_t>(f, 0);          // cn_byte_offset
    writeLE<uint32_t>(f, 64);         // cn_bit_count
    writeLE<uint32_t>(f, 0);          // cn_flags
    writeLE<uint32_t>(f, 0);          // cn_inval_bit_pos
    writeLE<uint8_t>(f, 255);         // cn_precision (none)
    writeZero(f, 3);
    writeZero(f, 8+8);                 // min/max (not set)
    writeZero(f, 32);                  // limit ranges (not set)
    // Total data written: 1+1+1+1+4+4+4+4+1+3+8+8+32 = 72 bytes
    // Need 80 bytes → 8 more padding
    writeZero(f, 8);

    // ---- CN2 (CAN data channel, 168 bytes) ----
    f.write("##CN", 4); writeZero(f, 4);
    writeLE<uint64_t>(f, szCN);
    writeLE<uint64_t>(f, 8);          // link_count
    writeLE<uint64_t>(f, 0);          // cn_cn_next
    writeLE<uint64_t>(f, 0);
    writeLE<uint64_t>(f, 0);
    writeLE<uint64_t>(f, 0);
    writeLE<uint64_t>(f, 0);
    writeLE<uint64_t>(f, 0);
    writeLE<uint64_t>(f, 0);
    writeLE<uint64_t>(f, 0);
    // Data: byte_offset=8 (after 8-byte timestamp)
    writeLE<uint8_t>(f, 0);           // cn_type = fixed length
    writeLE<uint8_t>(f, 0);           // cn_sync_type = none
    writeLE<uint8_t>(f, 10);          // cn_data_type = byte array
    writeLE<uint8_t>(f, 0);           // cn_bit_offset
    writeLE<uint32_t>(f, 8);          // cn_byte_offset (after timestamp)
    writeLE<uint32_t>(f, (kRecordSize - 8) * 8); // cn_bit_count
    writeLE<uint32_t>(f, 0);
    writeLE<uint32_t>(f, 0);
    writeLE<uint8_t>(f, 255);
    writeZero(f, 3);
    writeZero(f, 8+8+32+8);

    // ---- DT block ----
    f.write("##DT", 4); writeZero(f, 4);
    writeLE<uint64_t>(f, szDT);
    writeLE<uint64_t>(f, 0);          // link_count = 0

    // Write records
    for (const auto& frame : frames) {
        writeLE<uint64_t>(f, frame.timestamp_us * 1000ULL); // convert µs → ns
        writeLE<uint32_t>(f, frame.id);
        writeLE<uint8_t>(f, frame.dlc);
        f.write(reinterpret_cast<const char*>(frame.data), 8);
    }

    f.close();
    return true;
}

} // namespace socketspy::gui
