#include "j1939_diag.h"

namespace socketspy::protocols::j1939 {

std::vector<Dtc> decode_dm1(const uint8_t* data, uint16_t len) noexcept {
    std::vector<Dtc> dtcs;
    if (data == nullptr || len < 2)
        return dtcs;

    // First 2 bytes are lamp status bytes; skip them
    uint16_t offset = 2;
    while (static_cast<int32_t>(len) - offset >= 4) {
        const uint8_t* b = data + offset;
        Dtc dtc{};
        // SPN: lower 8 bits in b[0], next 8 in b[1], upper 3 bits in bits 0-2 of b[2]
        dtc.spn = static_cast<uint32_t>(b[0])
                | (static_cast<uint32_t>(b[1]) << 8)
                | ((static_cast<uint32_t>(b[2]) & 0x07u) << 16);
        // FMI: bits 7-3 of b[2]
        dtc.fmi        = (b[2] >> 3) & 0x1Fu;
        dtc.occurrence = b[3] & 0x7Fu;
        dtc.active     = (b[3] >> 7) & 1u;
        dtcs.push_back(dtc);
        offset = static_cast<uint16_t>(offset + 4u);
    }
    return dtcs;
}

std::string_view fmi_description(uint8_t fmi) noexcept {
    switch (fmi) {
        case 0:  return "Data valid but above normal operational range (most severe)";
        case 1:  return "Data valid but below normal operational range (most severe)";
        case 2:  return "Data erratic, intermittent or incorrect";
        case 3:  return "Voltage above normal or shorted high";
        case 4:  return "Voltage below normal or shorted low";
        case 5:  return "Current below normal or open circuit";
        case 6:  return "Current above normal or grounded circuit";
        case 7:  return "Mechanical system not responding properly";
        case 8:  return "Abnormal frequency, pulse width or period";
        case 9:  return "Abnormal update rate";
        case 10: return "Abnormal rate of change";
        case 11: return "Root cause not known";
        case 12: return "Bad intelligent device or component";
        case 13: return "Out of calibration";
        case 14: return "Special instructions";
        case 15: return "Data valid but above normal operational range (least severe)";
        case 16: return "Data valid but above normal operational range (moderately severe)";
        case 17: return "Data valid but below normal operational range (least severe)";
        case 18: return "Data valid but below normal operational range (moderately severe)";
        case 19: return "Received network data in error";
        case 20: return "Data drifted high";
        case 21: return "Data drifted low";
        case 31: return "Condition exists";
        default: return "Reserved";
    }
}

std::string_view lamp_status_string(uint8_t lamp_byte) noexcept {
    // Lamp byte bit layout per SAE J1939-73:
    // bits 7-6: Malfunction Indicator Lamp (MIL)
    // bits 5-4: Red Stop Lamp (RSL)
    // bits 3-2: Amber Warning Lamp (AWL)
    // bits 1-0: Protect Lamp (PL)
    // Value 00=off, 01=on, 10=error, 11=not available
    switch (lamp_byte & 0xC0u) {
        case 0x40: return "MIL:On";
        case 0x80: return "MIL:Error";
        default:   break;
    }
    // Return a simplified description of the dominant lamp state
    if ((lamp_byte & 0xC0u) == 0x40u) return "MIL On";
    if ((lamp_byte & 0x30u) == 0x10u) return "Red Stop Lamp On";
    if ((lamp_byte & 0x0Cu) == 0x04u) return "Amber Warning Lamp On";
    if ((lamp_byte & 0x03u) == 0x01u) return "Protect Lamp On";
    return "No Lamps Active";
}

} // namespace socketspy::protocols::j1939
