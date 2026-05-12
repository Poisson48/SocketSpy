#include "obd2.h"
#include <cstring>

namespace socketspy::protocols::obd2 {

std::optional<Obd2Response> decode_obd2(const uint8_t* data, uint8_t dlc) noexcept {
    // Standard OBD-II response on CAN:
    // data[0] = additional data bytes count (often 0x04 or varies)
    // data[1] = mode + 0x40 (response mode)
    // data[2] = PID
    // data[3..6] = data bytes A,B,C,D
    if (data == nullptr || dlc < 3)
        return std::nullopt;

    // data[0] should be 0x04 for a 4-byte response (mode+pid+2 data) or similar
    // We accept responses where data[1] is in range 0x41..0x49 (modes 01-09)
    const uint8_t raw_mode = data[1];
    if (raw_mode < 0x41u || raw_mode > 0x49u)
        return std::nullopt;

    Obd2Response resp{};
    resp.mode = static_cast<uint8_t>(raw_mode - 0x40u);
    resp.pid  = data[2];

    const uint8_t available = static_cast<uint8_t>(dlc >= 7 ? 4u : dlc - 3u);
    resp.data_len = available > 4u ? 4u : available;
    std::memset(resp.data, 0, sizeof(resp.data));
    for (uint8_t i = 0; i < resp.data_len; ++i)
        resp.data[i] = data[3 + i];

    return resp;
}

std::optional<double> interpret_pid(uint8_t mode, uint8_t pid,
                                     const uint8_t* data, uint8_t len) noexcept {
    if (data == nullptr || mode != 0x01)
        return std::nullopt;

    const uint8_t A = (len >= 1) ? data[0] : 0u;
    const uint8_t B = (len >= 2) ? data[1] : 0u;

    switch (pid) {
        case 0x04: return A / 2.55;                          // Engine load %
        case 0x05: return static_cast<double>(A) - 40.0;    // Coolant temp °C
        case 0x0A: return static_cast<double>(A) * 3.0;     // Fuel pressure kPa
        case 0x0B: return static_cast<double>(A);            // MAP pressure kPa
        case 0x0C: return (256.0 * A + B) / 4.0;            // RPM
        case 0x0D: return static_cast<double>(A);            // Speed km/h
        case 0x0F: return static_cast<double>(A) - 40.0;    // IAT °C
        case 0x10: return (256.0 * A + B) / 100.0;          // MAF g/s
        case 0x11: return A / 2.55;                          // Throttle pos %
        case 0x1F: return 256.0 * A + B;                     // Run time seconds
        case 0x21: return 256.0 * A + B;                     // Distance with MIL km
        case 0x2F: return A / 2.55;                          // Fuel level %
        default:   return std::nullopt;
    }
}

std::string_view pid_name(uint8_t mode, uint8_t pid) noexcept {
    if (mode != 0x01)
        return "Unknown";
    switch (pid) {
        case 0x00: return "PIDs supported [01-20]";
        case 0x01: return "Monitor status since DTCs cleared";
        case 0x03: return "Fuel system status";
        case 0x04: return "Calculated engine load";
        case 0x05: return "Engine coolant temperature";
        case 0x06: return "Short term fuel trim (bank 1)";
        case 0x07: return "Long term fuel trim (bank 1)";
        case 0x0A: return "Fuel pressure";
        case 0x0B: return "Intake manifold absolute pressure";
        case 0x0C: return "Engine RPM";
        case 0x0D: return "Vehicle speed";
        case 0x0E: return "Timing advance";
        case 0x0F: return "Intake air temperature";
        case 0x10: return "Mass air flow rate";
        case 0x11: return "Throttle position";
        case 0x12: return "Commanded secondary air status";
        case 0x1C: return "OBD standards this vehicle conforms to";
        case 0x1F: return "Run time since engine start";
        case 0x20: return "PIDs supported [21-40]";
        case 0x21: return "Distance traveled with MIL on";
        case 0x2F: return "Fuel tank level input";
        case 0x31: return "Distance traveled since codes cleared";
        case 0x33: return "Absolute barometric pressure";
        case 0x42: return "Control module voltage";
        case 0x46: return "Ambient air temperature";
        case 0x4D: return "Time run with MIL on";
        default:   return "Unknown PID";
    }
}

std::string_view pid_unit(uint8_t mode, uint8_t pid) noexcept {
    if (mode != 0x01)
        return "";
    switch (pid) {
        case 0x04: return "%";
        case 0x05: return "°C";
        case 0x0A: return "kPa";
        case 0x0B: return "kPa";
        case 0x0C: return "rpm";
        case 0x0D: return "km/h";
        case 0x0E: return "° before TDC";
        case 0x0F: return "°C";
        case 0x10: return "g/s";
        case 0x11: return "%";
        case 0x1F: return "s";
        case 0x21: return "km";
        case 0x2F: return "%";
        case 0x31: return "km";
        case 0x33: return "kPa";
        case 0x42: return "V";
        case 0x46: return "°C";
        case 0x4D: return "min";
        default:   return "";
    }
}

} // namespace socketspy::protocols::obd2
