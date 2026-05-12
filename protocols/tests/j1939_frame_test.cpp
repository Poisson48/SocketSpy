#include <gtest/gtest.h>
#include "j1939_frame.h"

using namespace socketspy::protocols::j1939;

// Helper: build a 29-bit J1939 CAN ID from components
static uint32_t make_id(uint8_t prio, bool dp, uint8_t pf, uint8_t ps, uint8_t sa) {
    return (static_cast<uint32_t>(prio & 0x07u) << 26)
         | (static_cast<uint32_t>(dp ? 1u : 0u) << 24)
         | (static_cast<uint32_t>(pf) << 16)
         | (static_cast<uint32_t>(ps) << 8)
         | sa;
}

// ---------------------------------------------------------------------------
// Decode tests
// ---------------------------------------------------------------------------

TEST(J1939Frame, DecodePriority) {
    uint32_t id = make_id(6, false, 0xFE, 0xCA, 0x00);
    J1939Id decoded = decode_j1939_id(id);
    EXPECT_EQ(decoded.priority, 6u);
}

TEST(J1939Frame, DecodeSA) {
    uint32_t id = make_id(6, false, 0xFE, 0xCA, 0x42);
    J1939Id decoded = decode_j1939_id(id);
    EXPECT_EQ(decoded.sa, 0x42u);
}

TEST(J1939Frame, DecodePF_PS) {
    uint32_t id = make_id(3, false, 0xFE, 0xCA, 0x11);
    J1939Id decoded = decode_j1939_id(id);
    EXPECT_EQ(decoded.pf, 0xFEu);
    EXPECT_EQ(decoded.ps, 0xCAu);
}

TEST(J1939Frame, DecodeDataPage) {
    uint32_t id = make_id(6, true, 0xFE, 0xCA, 0x00);
    J1939Id decoded = decode_j1939_id(id);
    EXPECT_TRUE(decoded.data_page);

    uint32_t id2 = make_id(6, false, 0xFE, 0xCA, 0x00);
    J1939Id decoded2 = decode_j1939_id(id2);
    EXPECT_FALSE(decoded2.data_page);
}

// DM1 PGN = 0xFECA — broadcast, pf=0xFE >= 0xF0
TEST(J1939Frame, DecodeDM1_PGN) {
    uint32_t id = make_id(6, false, 0xFEu, 0xCAu, 0x00u);
    J1939Id decoded = decode_j1939_id(id);
    EXPECT_FALSE(decoded.peer_to_peer);
    EXPECT_EQ(decoded.pgn, 0xFECAu);
}

// ETC1 PGN = 0xF001 — broadcast
TEST(J1939Frame, DecodeETC1_PGN) {
    uint32_t id = make_id(6, false, 0xF0u, 0x01u, 0x03u);
    J1939Id decoded = decode_j1939_id(id);
    EXPECT_FALSE(decoded.peer_to_peer);
    EXPECT_EQ(decoded.pgn, 0xF001u);
}

// Peer-to-peer: pf < 0xF0
TEST(J1939Frame, DecodePeerToPeer) {
    uint32_t id = make_id(6, false, 0xECu, 0x05u, 0x03u);
    J1939Id decoded = decode_j1939_id(id);
    EXPECT_TRUE(decoded.peer_to_peer);
    // PGN excludes PS (destination address)
    EXPECT_EQ(decoded.pgn, 0xEC00u);
    EXPECT_EQ(decoded.ps, 0x05u);  // destination address
}

// ---------------------------------------------------------------------------
// Encode round-trip tests
// ---------------------------------------------------------------------------

TEST(J1939Frame, EncodeDecodRoundTrip_DM1) {
    uint32_t original = make_id(6, false, 0xFEu, 0xCAu, 0x11u);
    J1939Id decoded = decode_j1939_id(original);
    uint32_t reencoded = encode_j1939_id(decoded);
    EXPECT_EQ(reencoded, original);
}

TEST(J1939Frame, EncodeDecodeRoundTrip_TP_CM) {
    uint32_t original = make_id(7, false, 0xECu, 0xFFu, 0x22u);
    J1939Id decoded = decode_j1939_id(original);
    uint32_t reencoded = encode_j1939_id(decoded);
    EXPECT_EQ(reencoded, original);
}

TEST(J1939Frame, EncodeDecodeRoundTrip_AllPriorities) {
    for (uint8_t prio = 0; prio <= 7; ++prio) {
        uint32_t original = make_id(prio, false, 0xFEu, 0xEEu, 0x05u);
        J1939Id decoded = decode_j1939_id(original);
        EXPECT_EQ(decoded.priority, prio);
        uint32_t reencoded = encode_j1939_id(decoded);
        EXPECT_EQ(reencoded, original);
    }
}

// ---------------------------------------------------------------------------
// PGN name lookup
// ---------------------------------------------------------------------------

TEST(J1939Frame, PgnNames) {
    EXPECT_EQ(pgn_name(0xFECA), "DM1 - Active Diagnostic Trouble Codes");
    EXPECT_EQ(pgn_name(0xFECB), "DM2 - Previously Active DTCs");
    EXPECT_EQ(pgn_name(0xF001), "ETC1 - Electronic Transmission Controller 1");
    EXPECT_EQ(pgn_name(0xFEF1), "CCVS - Cruise Control Vehicle Speed");
    EXPECT_EQ(pgn_name(0xFEEE), "ET1 - Engine Temperature 1");
}

TEST(J1939Frame, PgnNameUnknown) {
    EXPECT_EQ(pgn_name(0x1234), "Unknown PGN");
}

// ---------------------------------------------------------------------------
// Address claim names
// ---------------------------------------------------------------------------

TEST(J1939Frame, AddressClaimNames) {
    EXPECT_EQ(address_claim_name(0),   "Engine #1");
    EXPECT_EQ(address_claim_name(3),   "Transmission #1");
    EXPECT_EQ(address_claim_name(254), "Global (broadcast)");
    EXPECT_EQ(address_claim_name(255), "Global (all nodes)");
}
