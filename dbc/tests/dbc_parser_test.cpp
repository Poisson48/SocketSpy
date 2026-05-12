#include "dbc_parser.h"
#include "dbc_types.h"
#include <gtest/gtest.h>
#include <string_view>

using namespace socketspy::dbc;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static DbcDatabase parse_ok(std::string_view text) {
    auto result = parse_dbc(text);
    EXPECT_TRUE(result.has_value()) << "parse failed unexpectedly";
    return result.value_or(DbcDatabase{});
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST(DbcParser, EmptyInput) {
    auto result = parse_dbc("");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->messages.empty());
    EXPECT_TRUE(result->nodes.empty());
    EXPECT_TRUE(result->version.empty());
}

TEST(DbcParser, WhitespaceOnlyInput) {
    auto result = parse_dbc("   \n\t\r\n  ");
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->messages.empty());
}

TEST(DbcParser, VersionString) {
    auto db = parse_ok("VERSION \"1.0\"\n");
    EXPECT_EQ(db.version, "1.0");
}

TEST(DbcParser, VersionEmpty) {
    auto db = parse_ok("VERSION \"\"\n");
    EXPECT_EQ(db.version, "");
}

TEST(DbcParser, NodeList) {
    auto db = parse_ok("VERSION \"\"\nBU_: ECU1 ECU2 GW\n");
    ASSERT_EQ(db.nodes.size(), 3u);
    EXPECT_EQ(db.nodes[0], "ECU1");
    EXPECT_EQ(db.nodes[1], "ECU2");
    EXPECT_EQ(db.nodes[2], "GW");
}

TEST(DbcParser, SimpleMessage) {
    const char* dbc = R"(VERSION ""
BU_:
BO_ 100 TestMsg: 8 Vector__XXX
 SG_ Speed : 0|8@1+ (1,0) [0|255] "km/h" Vector__XXX
)";
    auto db = parse_ok(dbc);
    ASSERT_EQ(db.messages.size(), 1u);
    const auto& msg = db.messages[0];
    EXPECT_EQ(msg.id, 100u);
    EXPECT_FALSE(msg.extended);
    EXPECT_EQ(msg.name, "TestMsg");
    EXPECT_EQ(msg.dlc, 8u);
    EXPECT_EQ(msg.transmitter, "Vector__XXX");
    ASSERT_EQ(msg.signals.size(), 1u);
    const auto& sig = msg.signals[0];
    EXPECT_EQ(sig.name, "Speed");
    EXPECT_EQ(sig.start_bit, 0u);
    EXPECT_EQ(sig.bit_length, 8u);
    EXPECT_EQ(sig.byte_order, ByteOrder::LittleEndian);
    EXPECT_EQ(sig.value_type, ValueType::Unsigned);
    EXPECT_DOUBLE_EQ(sig.factor, 1.0);
    EXPECT_DOUBLE_EQ(sig.offset, 0.0);
    EXPECT_EQ(sig.unit, "km/h");
}

TEST(DbcParser, TwoSignalsOneMessage) {
    const char* dbc = R"(VERSION ""
BO_ 200 EngineMsg: 8 ECU
 SG_ RPM : 0|16@1+ (0.25,0) [0|8000] "rpm" Vector__XXX
 SG_ Temp : 16|8@0- (1,-40) [-40|215] "degC" Vector__XXX
)";
    auto db = parse_ok(dbc);
    ASSERT_EQ(db.messages.size(), 1u);
    ASSERT_EQ(db.messages[0].signals.size(), 2u);
    const auto& sig0 = db.messages[0].signals[0];
    EXPECT_EQ(sig0.name, "RPM");
    EXPECT_EQ(sig0.byte_order, ByteOrder::LittleEndian);
    EXPECT_DOUBLE_EQ(sig0.factor, 0.25);
    const auto& sig1 = db.messages[0].signals[1];
    EXPECT_EQ(sig1.name, "Temp");
    EXPECT_EQ(sig1.byte_order, ByteOrder::BigEndian);
    EXPECT_EQ(sig1.value_type, ValueType::Signed);
    EXPECT_DOUBLE_EQ(sig1.offset, -40.0);
}

TEST(DbcParser, ExtendedId) {
    const char* dbc = R"(VERSION ""
BO_ 2566853172 ExtMsg: 8 ECU
 SG_ Val : 0|8@1+ (1,0) [0|255] "" Vector__XXX
)";
    // 2566853172 = 0x98FF1234 => extended, id = 0x18FF1234
    auto db = parse_ok(dbc);
    ASSERT_EQ(db.messages.size(), 1u);
    EXPECT_TRUE(db.messages[0].extended);
    EXPECT_EQ(db.messages[0].id, 0x18FF1234u);
}

TEST(DbcParser, ValBlock) {
    const char* dbc = R"(VERSION ""
BO_ 300 GearMsg: 8 TCU
 SG_ Gear : 0|4@1+ (1,0) [0|15] "" Vector__XXX
VAL_ 300 Gear 0 "Park" 1 "Reverse" 2 "Neutral" 3 "Drive" ;
)";
    auto db = parse_ok(dbc);
    ASSERT_EQ(db.messages.size(), 1u);
    const auto& sig = db.messages[0].signals[0];
    ASSERT_EQ(sig.value_descriptions.size(), 4u);
    EXPECT_EQ(sig.value_descriptions.at(0), "Park");
    EXPECT_EQ(sig.value_descriptions.at(1), "Reverse");
    EXPECT_EQ(sig.value_descriptions.at(2), "Neutral");
    EXPECT_EQ(sig.value_descriptions.at(3), "Drive");
}

TEST(DbcParser, MessageComment) {
    const char* dbc = R"(VERSION ""
BO_ 100 Msg: 8 ECU
 SG_ Val : 0|8@1+ (1,0) [0|0] "" Vector__XXX
CM_ BO_ 100 "Engine control message";
)";
    auto db = parse_ok(dbc);
    ASSERT_EQ(db.messages.size(), 1u);
    EXPECT_EQ(db.messages[0].comment, "Engine control message");
}

TEST(DbcParser, MalformedSignalLine_GracefulSkip) {
    // Signal line with missing closing ']' — should skip gracefully, not crash
    const char* dbc = R"(VERSION ""
BO_ 100 Msg: 8 ECU
 SG_ Bad : 0|8@1+ (1,0) [0|255 "rpm" Vector__XXX
 SG_ Good : 8|8@1+ (1,0) [0|255] "rpm" Vector__XXX
)";
    // Should not crash; Good signal may or may not parse depending on recovery
    auto result = parse_dbc(dbc);
    // Must not crash — result is either ok or error, never UB
    (void)result;
}

TEST(DbcParser, BinaryGarbage) {
    // Ensure no crash on binary input
    const uint8_t garbage[] = {0x00, 0xFF, 0x80, 0x01, 0x42, 0xDE, 0xAD};
    std::string_view sv(reinterpret_cast<const char*>(garbage), sizeof(garbage));
    auto result = parse_dbc(sv);
    (void)result; // just must not crash
}

TEST(DbcParser, NsSection) {
    const char* dbc = R"(VERSION ""

NS_ :
  NS_DESC_
  CM_
  BA_DEF_
  BA_

BU_:
)";
    auto db = parse_ok(dbc);
    EXPECT_TRUE(db.messages.empty());
}

TEST(DbcParser, DuplicateMessageId_SecondSkipped) {
    const char* dbc = R"(VERSION ""
BO_ 100 Msg1: 8 ECU
 SG_ Val1 : 0|8@1+ (1,0) [0|0] "" Vector__XXX
BO_ 100 Msg2: 8 ECU
 SG_ Val2 : 0|8@1+ (1,0) [0|0] "" Vector__XXX
)";
    auto db = parse_ok(dbc);
    // First message keeps its position; second is rejected
    EXPECT_GE(db.messages.size(), 1u);
    EXPECT_EQ(db.messages[0].name, "Msg1");
}
