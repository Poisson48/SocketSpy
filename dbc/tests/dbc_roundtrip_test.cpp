#include "dbc_parser.h"
#include "dbc_writer.h"
#include "dbc_types.h"
#include <gtest/gtest.h>
#include <cmath>
#include <string>

using namespace socketspy::dbc;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static DbcDatabase parse_ok(const std::string& text) {
    auto result = parse_dbc(text);
    EXPECT_TRUE(result.has_value());
    return result.value_or(DbcDatabase{});
}

static DbcDatabase roundtrip(const std::string& text) {
    auto db1 = parse_ok(text);
    auto written = write_dbc(db1);
    return parse_ok(written);
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST(DbcRoundtrip, MessageCount) {
    const char* dbc = R"(VERSION ""
BU_:
BO_ 100 Msg1: 8 ECU
 SG_ V1 : 0|8@1+ (1,0) [0|255] "" Vector__XXX
BO_ 200 Msg2: 4 GW
 SG_ V2 : 0|16@1+ (0.1,0) [0|100] "V" Vector__XXX
)";
    auto db2 = roundtrip(dbc);
    EXPECT_EQ(db2.messages.size(), 2u);
}

TEST(DbcRoundtrip, SignalNames) {
    const char* dbc = R"(VERSION ""
BO_ 100 Msg: 8 ECU
 SG_ Alpha : 0|8@1+ (1,0) [0|0] "" Vector__XXX
 SG_ Beta  : 8|8@1+ (1,0) [0|0] "" Vector__XXX
)";
    auto db2 = roundtrip(dbc);
    ASSERT_EQ(db2.messages.size(), 1u);
    ASSERT_EQ(db2.messages[0].signals.size(), 2u);
    EXPECT_EQ(db2.messages[0].signals[0].name, "Alpha");
    EXPECT_EQ(db2.messages[0].signals[1].name, "Beta");
}

TEST(DbcRoundtrip, StartBits) {
    const char* dbc = R"(VERSION ""
BO_ 100 Msg: 8 ECU
 SG_ S1 : 0|8@1+ (1,0) [0|0] "" Vector__XXX
 SG_ S2 : 8|16@1+ (1,0) [0|0] "" Vector__XXX
 SG_ S3 : 24|8@0+ (1,0) [0|0] "" Vector__XXX
)";
    auto db1 = parse_ok(dbc);
    auto written = write_dbc(db1);
    auto db2 = parse_ok(written);
    ASSERT_EQ(db2.messages.size(), 1u);
    ASSERT_EQ(db2.messages[0].signals.size(), 3u);
    EXPECT_EQ(db2.messages[0].signals[0].start_bit, 0u);
    EXPECT_EQ(db2.messages[0].signals[1].start_bit, 8u);
    EXPECT_EQ(db2.messages[0].signals[2].start_bit, 24u);
}

TEST(DbcRoundtrip, ExtendedIdSurvives) {
    const char* dbc = R"(VERSION ""
BO_ 2566853172 ExtMsg: 8 ECU
 SG_ Val : 0|8@1+ (1,0) [0|255] "" Vector__XXX
)";
    // 2566853172 = 0x98FF1234
    auto db1 = parse_ok(dbc);
    ASSERT_TRUE(db1.messages[0].extended);

    auto written = write_dbc(db1);
    auto db2 = parse_ok(written);
    ASSERT_EQ(db2.messages.size(), 1u);
    EXPECT_TRUE(db2.messages[0].extended);
    EXPECT_EQ(db2.messages[0].id, db1.messages[0].id);
}

TEST(DbcRoundtrip, FactorOffsetEpsilon) {
    const char* dbc = R"(VERSION ""
BO_ 100 Msg: 8 ECU
 SG_ Temp : 0|16@1- (0.03125,-273.15) [-273.15|1000] "K" Vector__XXX
)";
    auto db1 = parse_ok(dbc);
    auto written = write_dbc(db1);
    auto db2 = parse_ok(written);
    ASSERT_EQ(db2.messages.size(), 1u);
    ASSERT_EQ(db2.messages[0].signals.size(), 1u);
    const auto& sig1 = db1.messages[0].signals[0];
    const auto& sig2 = db2.messages[0].signals[0];
    EXPECT_NEAR(sig2.factor,  sig1.factor,  1e-7);
    EXPECT_NEAR(sig2.offset,  sig1.offset,  1e-5);
    EXPECT_NEAR(sig2.min_val, sig1.min_val, 1e-5);
}

TEST(DbcRoundtrip, ValDescriptionsSurvive) {
    const char* dbc = R"(VERSION ""
BO_ 300 GearMsg: 8 TCU
 SG_ Gear : 0|4@1+ (1,0) [0|15] "" Vector__XXX
VAL_ 300 Gear 0 "Park" 1 "Reverse" 2 "Neutral" 3 "Drive" ;
)";
    auto db1 = parse_ok(dbc);
    auto written = write_dbc(db1);
    auto db2 = parse_ok(written);
    ASSERT_EQ(db2.messages.size(), 1u);
    ASSERT_EQ(db2.messages[0].signals.size(), 1u);
    const auto& vd = db2.messages[0].signals[0].value_descriptions;
    ASSERT_EQ(vd.size(), 4u);
    EXPECT_EQ(vd.at(0), "Park");
    EXPECT_EQ(vd.at(3), "Drive");
}

TEST(DbcRoundtrip, VersionSurvives) {
    const char* dbc = "VERSION \"2.0\"\n\nBU_:\n\n";
    auto db2 = roundtrip(dbc);
    EXPECT_EQ(db2.version, "2.0");
}

TEST(DbcRoundtrip, NodesSurvive) {
    const char* dbc = "VERSION \"\"\nBU_: ECU1 ECU2 GW\n";
    auto db2 = roundtrip(dbc);
    ASSERT_EQ(db2.nodes.size(), 3u);
    EXPECT_EQ(db2.nodes[0], "ECU1");
    EXPECT_EQ(db2.nodes[1], "ECU2");
    EXPECT_EQ(db2.nodes[2], "GW");
}

TEST(DbcRoundtrip, ByteOrderPreserved) {
    const char* dbc = R"(VERSION ""
BO_ 100 Msg: 8 ECU
 SG_ LE_sig : 0|8@1+ (1,0) [0|0] "" Vector__XXX
 SG_ BE_sig : 7|8@0+ (1,0) [0|0] "" Vector__XXX
)";
    auto db2 = roundtrip(dbc);
    ASSERT_EQ(db2.messages[0].signals.size(), 2u);
    EXPECT_EQ(db2.messages[0].signals[0].byte_order, ByteOrder::LittleEndian);
    EXPECT_EQ(db2.messages[0].signals[1].byte_order, ByteOrder::BigEndian);
}
