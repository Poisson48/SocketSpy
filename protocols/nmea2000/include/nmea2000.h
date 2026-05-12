#pragma once
#include <cstdint>
#include <string_view>
#include <optional>

namespace socketspy::protocols::nmea2000 {

// Common NMEA 2000 PGNs
enum class Pgn : uint32_t {
    SystemTime        = 126992,
    VesselHeading     = 127250,
    RateOfTurn        = 127251,
    VehicleSpeed      = 128259,
    WaterDepth        = 128267,
    GnssDop           = 129539,
    GnssPosition      = 129025,
    WindSpeed         = 130306,
    OutsideTemp       = 130312,
};

struct Nmea2000Frame {
    uint32_t pgn;
    uint8_t  priority;
    uint8_t  src;
    uint8_t  dst;
    uint8_t  data[8];
    uint8_t  dlc;
};

// Decode a J1939-style 29-bit ID for NMEA 2000.
Nmea2000Frame decode_nmea2000_id(uint32_t can_id, const uint8_t* data,
                                   uint8_t dlc) noexcept;

std::string_view pgn_name(uint32_t pgn) noexcept;

// Interpret known single-frame PGNs. Returns a human-readable string.
std::optional<double> extract_vessel_heading_rad(const uint8_t* data, uint8_t dlc) noexcept;
std::optional<double> extract_water_depth_m(const uint8_t* data, uint8_t dlc) noexcept;
std::optional<double> extract_wind_speed_ms(const uint8_t* data, uint8_t dlc) noexcept;

} // namespace socketspy::protocols::nmea2000
