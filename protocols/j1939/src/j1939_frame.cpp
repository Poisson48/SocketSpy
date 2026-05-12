#include "j1939_frame.h"

namespace socketspy::protocols::j1939 {

J1939Id decode_j1939_id(uint32_t can_id) noexcept {
    J1939Id id{};
    id.priority   = static_cast<uint8_t>((can_id >> 26) & 0x07u);
    id.reserved   = (can_id >> 25) & 1u;
    id.data_page  = (can_id >> 24) & 1u;
    id.pf         = static_cast<uint8_t>((can_id >> 16) & 0xFFu);
    id.ps         = static_cast<uint8_t>((can_id >> 8)  & 0xFFu);
    id.sa         = static_cast<uint8_t>(can_id & 0xFFu);

    id.peer_to_peer = (id.pf < 0xF0u);
    // PGN includes DP bit, PF, and (if peer-to-peer) PS=0, else PS=group extension
    uint32_t dp = id.data_page ? 1u : 0u;
    if (id.peer_to_peer)
        id.pgn = (dp << 17) | (static_cast<uint32_t>(id.pf) << 8);
    else
        id.pgn = (dp << 17) | (static_cast<uint32_t>(id.pf) << 8) | id.ps;

    return id;
}

uint32_t encode_j1939_id(const J1939Id& id) noexcept {
    uint32_t can_id = 0;
    can_id |= static_cast<uint32_t>(id.priority & 0x07u) << 26;
    can_id |= static_cast<uint32_t>(id.reserved  ? 1u : 0u) << 25;
    can_id |= static_cast<uint32_t>(id.data_page ? 1u : 0u) << 24;
    can_id |= static_cast<uint32_t>(id.pf)  << 16;
    can_id |= static_cast<uint32_t>(id.ps)  << 8;
    can_id |= static_cast<uint32_t>(id.sa);
    return can_id;
}

std::string_view pgn_name(uint32_t pgn) noexcept {
    switch (pgn) {
        case 0xFECA: return "DM1 - Active Diagnostic Trouble Codes";
        case 0xFECB: return "DM2 - Previously Active DTCs";
        case 0xFECC: return "DM3 - Diagnostic Data Clear/Reset";
        case 0xFECD: return "DM4 - Freeze Frame Parameters";
        case 0xFECE: return "DM5 - Diagnostic Readiness 1";
        case 0xFECF: return "DM6 - Emission Related Pending DTCs";
        case 0xF001: return "ETC1 - Electronic Transmission Controller 1";
        case 0xF002: return "ETC2 - Electronic Transmission Controller 2";
        case 0xFEF1: return "CCVS - Cruise Control Vehicle Speed";
        case 0xFEEE: return "ET1 - Engine Temperature 1";
        case 0xFEEF: return "EFL/P1 - Engine Fluid Level/Pressure 1";
        case 0xFEF2: return "LFC - Fuel Consumption (Liquid)";
        case 0xFEF5: return "AMB - Ambient Conditions";
        case 0xFEF6: return "HOURS - Engine Hours/Revolutions";
        case 0xFEF7: return "VD - Vehicle Distance";
        case 0xFEF8: return "SHUTDN - Shutdown";
        case 0xFEF9: return "LFE - Fuel Economy (Liquid)";
        case 0xFEFA: return "EI - Engine Identifier";
        case 0xFEFC: return "VH - Vehicle Identification";
        case 0xFF00: return "Proprietary A";
        case 0xEF00: return "Proprietary B";
        case 0xFED5: return "AT1T1I - Aftertreatment 1 Diesel Exhaust Fluid Tank 1 Information";
        case 0xFDA4: return "EEC3 - Electronic Engine Controller 3";
        case 0xF003: return "EEC1 - Electronic Engine Controller 1";
        case 0xF004: return "EEC2 - Electronic Engine Controller 2";
        case 0xFEC0: return "DD - Dash Display";
        case 0xFEB6: return "AI1 - Alternator Information";
        case 0xFEBF: return "IC1 - Inlet/Exhaust Conditions 1";
        case 0xFEAE: return "EC1 - Engine Configuration 1";
        case 0xFEDF: return "EPOS - Electronic Positioning System (EPS)";
        case 0xEE00: return "Address Claimed";
        case 0xEC00: return "TP.CM - Transport Protocol Connection Management";
        case 0xEB00: return "TP.DT - Transport Protocol Data Transfer";
        default:     return "Unknown PGN";
    }
}

std::string_view address_claim_name(uint8_t sa) noexcept {
    switch (sa) {
        case 0:   return "Engine #1";
        case 1:   return "Engine #2";
        case 3:   return "Transmission #1";
        case 4:   return "Transmission #2";
        case 11:  return "Brakes - System Controller";
        case 17:  return "Cruise Control";
        case 21:  return "Instrument Cluster #1";
        case 23:  return "Tire Pressure Controller";
        case 33:  return "Body Controller";
        case 40:  return "Cab Display #1";
        case 49:  return "Cab Controller - Primary";
        case 128: return "Tractor/Primary Machine #1";
        case 129: return "Tractor/Primary Machine #2";
        case 130: return "Tractor/Primary Machine #3";
        case 247: return "Null Address";
        case 248: return "Industry Group 1 specific";
        case 249: return "Industry Group 2 specific";
        case 253: return "On-Board Diagnostics";
        case 254: return "Global (broadcast)";
        case 255: return "Global (all nodes)";
        default:  return "Unknown Address";
    }
}

} // namespace socketspy::protocols::j1939
