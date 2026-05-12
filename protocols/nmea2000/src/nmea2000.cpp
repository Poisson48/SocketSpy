#include "nmea2000.h"
#include <cstring>

namespace socketspy::protocols::nmea2000 {

// NMEA 2000 uses the J1939 frame structure with 29-bit CAN IDs.
// Priority: bits 28-26, Reserved: bit 25, DataPage: bit 24,
// PF: bits 23-16, PS: bits 15-8, SA: bits 7-0

Nmea2000Frame decode_nmea2000_id(uint32_t can_id, const uint8_t* payload,
                                   uint8_t dlc) noexcept {
    Nmea2000Frame f{};
    f.priority = static_cast<uint8_t>((can_id >> 26) & 0x07u);
    const bool dp = (can_id >> 24) & 1u;
    const uint8_t pf = static_cast<uint8_t>((can_id >> 16) & 0xFFu);
    const uint8_t ps = static_cast<uint8_t>((can_id >> 8) & 0xFFu);
    f.src = static_cast<uint8_t>(can_id & 0xFFu);

    if (pf < 0xF0u) {
        // Peer-to-peer (destination address in PS)
        f.dst = ps;
        f.pgn = (static_cast<uint32_t>(dp ? 1u : 0u) << 17)
              | (static_cast<uint32_t>(pf) << 8);
    } else {
        f.dst = 0xFF;  // broadcast
        f.pgn = (static_cast<uint32_t>(dp ? 1u : 0u) << 17)
              | (static_cast<uint32_t>(pf) << 8)
              | ps;
    }

    f.dlc = dlc > 8u ? 8u : dlc;
    if (payload != nullptr)
        std::memcpy(f.data, payload, f.dlc);
    return f;
}

std::string_view pgn_name(uint32_t pgn) noexcept {
    switch (static_cast<Pgn>(pgn)) {
        case Pgn::SystemTime:    return "System Time";
        case Pgn::VesselHeading: return "Vessel Heading";
        case Pgn::RateOfTurn:    return "Rate of Turn";
        case Pgn::VehicleSpeed:  return "Speed";
        case Pgn::WaterDepth:    return "Water Depth";
        case Pgn::GnssDop:       return "GNSS DOPs";
        case Pgn::GnssPosition:  return "GNSS Position Data";
        case Pgn::WindSpeed:     return "Wind Data";
        case Pgn::OutsideTemp:   return "Environmental Parameters";
        default:                 return "Unknown PGN";
    }
}

std::optional<double> extract_vessel_heading_rad(const uint8_t* data,
                                                   uint8_t dlc) noexcept {
    // PGN 127250: byte 1-2 = heading as LE uint16 * 1e-4 rad
    if (data == nullptr || dlc < 3)
        return std::nullopt;
    const uint16_t raw = static_cast<uint16_t>(data[1] | (data[2] << 8));
    if (raw == 0xFFFFu)  // not available sentinel
        return std::nullopt;
    return static_cast<double>(raw) * 1e-4;
}

std::optional<double> extract_water_depth_m(const uint8_t* data,
                                              uint8_t dlc) noexcept {
    // PGN 128267: bytes 0-3 = depth as LE uint32 * 0.01 m
    //             byte 4     = offset (signed int8) * 0.1 m (transducer offset)
    if (data == nullptr || dlc < 4)
        return std::nullopt;
    const uint32_t raw = static_cast<uint32_t>(data[0])
                       | (static_cast<uint32_t>(data[1]) << 8)
                       | (static_cast<uint32_t>(data[2]) << 16)
                       | (static_cast<uint32_t>(data[3]) << 24);
    if (raw == 0xFFFFFFFFu)
        return std::nullopt;
    double depth = static_cast<double>(raw) * 0.01;
    if (dlc >= 5) {
        const int8_t offset = static_cast<int8_t>(data[4]);
        depth += static_cast<double>(offset) * 0.1;
    }
    return depth;
}

std::optional<double> extract_wind_speed_ms(const uint8_t* data,
                                              uint8_t dlc) noexcept {
    // PGN 130306: bytes 0-1 = wind speed as LE uint16 * 0.01 m/s
    if (data == nullptr || dlc < 2)
        return std::nullopt;
    const uint16_t raw = static_cast<uint16_t>(data[0] | (data[1] << 8));
    if (raw == 0xFFFFu)
        return std::nullopt;
    return static_cast<double>(raw) * 0.01;
}

} // namespace socketspy::protocols::nmea2000
