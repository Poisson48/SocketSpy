#include "dbc_signal.h"
#include "dbc_types.h"
#include <gtest/gtest.h>
#include <array>
#include <cmath>

using namespace socketspy::dbc;

// ---------------------------------------------------------------------------
// Helpers to build signals quickly
// ---------------------------------------------------------------------------

static Signal make_le(uint8_t start, uint8_t len,
                       ValueType vt = ValueType::Unsigned,
                       double factor = 1.0, double offset = 0.0,
                       double mn = 0.0, double mx = 0.0) {
    Signal s;
    s.start_bit  = start;
    s.bit_length = len;
    s.byte_order = ByteOrder::LittleEndian;
    s.value_type = vt;
    s.factor     = factor;
    s.offset     = offset;
    s.min_val    = mn;
    s.max_val    = mx;
    return s;
}

static Signal make_be(uint8_t start, uint8_t len,
                       ValueType vt = ValueType::Unsigned,
                       double factor = 1.0, double offset = 0.0) {
    Signal s;
    s.start_bit  = start;
    s.bit_length = len;
    s.byte_order = ByteOrder::BigEndian;
    s.value_type = vt;
    s.factor     = factor;
    s.offset     = offset;
    return s;
}

// ---------------------------------------------------------------------------
// Little-endian tests
// ---------------------------------------------------------------------------

TEST(DbcSignal, LE_Uint8_Byte0) {
    auto sig = make_le(0, 8);
    std::array<uint8_t, 1> data = {0xAB};
    auto val = extract_signal(sig, data);
    ASSERT_TRUE(val.has_value());
    EXPECT_DOUBLE_EQ(*val, 171.0); // 0xAB = 171
}

TEST(DbcSignal, LE_Uint16_TwoBytes) {
    auto sig = make_le(0, 16);
    std::array<uint8_t, 2> data = {0x34, 0x12};
    auto val = extract_signal(sig, data);
    ASSERT_TRUE(val.has_value());
    EXPECT_DOUBLE_EQ(*val, 0x1234); // 4660
}

TEST(DbcSignal, LE_Int8_Signed_Minus1) {
    auto sig = make_le(0, 8, ValueType::Signed);
    std::array<uint8_t, 1> data = {0xFF};
    auto val = extract_signal(sig, data);
    ASSERT_TRUE(val.has_value());
    EXPECT_DOUBLE_EQ(*val, -1.0);
}

TEST(DbcSignal, LE_Int8_Signed_Minus128) {
    auto sig = make_le(0, 8, ValueType::Signed);
    std::array<uint8_t, 1> data = {0x80};
    auto val = extract_signal(sig, data);
    ASSERT_TRUE(val.has_value());
    EXPECT_DOUBLE_EQ(*val, -128.0);
}

TEST(DbcSignal, LE_FactorOffset) {
    // factor=0.1, offset=-40, raw=650 -> 65.0 - 40 = 25.0
    auto sig = make_le(0, 16, ValueType::Unsigned, 0.1, -40.0);
    // raw = 650 = 0x028A
    std::array<uint8_t, 2> data = {0x8A, 0x02};
    auto val = extract_signal(sig, data);
    ASSERT_TRUE(val.has_value());
    EXPECT_NEAR(*val, 25.0, 1e-9);
}

TEST(DbcSignal, LE_TooShortData_Nullopt) {
    auto sig = make_le(0, 16);
    std::array<uint8_t, 1> data = {0x34}; // need 2 bytes
    auto val = extract_signal(sig, data);
    EXPECT_FALSE(val.has_value());
}

TEST(DbcSignal, LE_EmptyData_Nullopt) {
    auto sig = make_le(0, 8);
    auto val = extract_signal(sig, std::span<const uint8_t>{});
    EXPECT_FALSE(val.has_value());
}

TEST(DbcSignal, LE_4BitNibble) {
    auto sig = make_le(4, 4); // bits 4..7 of byte 0
    std::array<uint8_t, 1> data = {0xA0}; // upper nibble = 0xA = 10
    auto raw = extract_raw(sig, data);
    ASSERT_TRUE(raw.has_value());
    EXPECT_EQ(*raw, 10);
}

TEST(DbcSignal, LE_Uint32_FourBytes) {
    auto sig = make_le(0, 32);
    std::array<uint8_t, 4> data = {0x78, 0x56, 0x34, 0x12};
    auto raw = extract_raw(sig, data);
    ASSERT_TRUE(raw.has_value());
    EXPECT_EQ(*raw, 0x12345678);
}

// ---------------------------------------------------------------------------
// Big-endian tests
// ---------------------------------------------------------------------------

TEST(DbcSignal, BE_Uint8_StartBit7) {
    // BE start_bit=7: MSB at byte0 bit7, 8 bits -> covers byte0 entirely
    // DBC bit numbering: byte0 = bits 7..0
    // start=7, len=8: bits 7,6,5,4,3,2,1,0 of byte0 = full byte0
    auto sig = make_be(7, 8);
    std::array<uint8_t, 1> data = {0x42};
    auto raw = extract_raw(sig, data);
    ASSERT_TRUE(raw.has_value());
    EXPECT_EQ(*raw, 0x42);
}

TEST(DbcSignal, BE_Uint16_KnownVector) {
    // DBC big-endian 16-bit starting at bit 7 (MSB at byte0 bit7):
    // bits: 7,6,5,4,3,2,1,0,15,14,13,12,11,10,9,8
    // = byte0[7..0], byte1[7..0] => standard big-endian word
    auto sig = make_be(7, 16);
    std::array<uint8_t, 2> data = {0x12, 0x34};
    auto raw = extract_raw(sig, data);
    ASSERT_TRUE(raw.has_value());
    EXPECT_EQ(*raw, 0x1234);
}

TEST(DbcSignal, BE_TooShort_Nullopt) {
    auto sig = make_be(7, 16);
    std::array<uint8_t, 1> data = {0x12};
    auto val = extract_signal(sig, data);
    EXPECT_FALSE(val.has_value());
}

TEST(DbcSignal, BE_Signed_Negative) {
    // 8-bit signed BE at start 7 — value 0xFF -> -1
    auto sig = make_be(7, 8, ValueType::Signed);
    std::array<uint8_t, 1> data = {0xFF};
    auto raw = extract_raw(sig, data);
    ASSERT_TRUE(raw.has_value());
    EXPECT_EQ(*raw, -1);
}

// ---------------------------------------------------------------------------
// Pack tests
// ---------------------------------------------------------------------------

TEST(DbcSignal, Pack_LE_Uint8_RoundTrip) {
    auto sig = make_le(0, 8, ValueType::Unsigned, 1.0, 0.0, 0.0, 255.0);
    std::array<uint8_t, 1> data = {0};
    EXPECT_TRUE(pack_signal(sig, 171.0, data));
    auto val = extract_signal(sig, data);
    ASSERT_TRUE(val.has_value());
    EXPECT_DOUBLE_EQ(*val, 171.0);
}

TEST(DbcSignal, Pack_LE_OutOfRange_ReturnsFalse) {
    auto sig = make_le(0, 8, ValueType::Unsigned, 1.0, 0.0, 0.0, 255.0);
    std::array<uint8_t, 1> data = {0};
    EXPECT_FALSE(pack_signal(sig, 300.0, data));
}

TEST(DbcSignal, Pack_BE_Uint8_RoundTrip) {
    auto sig = make_be(7, 8);
    std::array<uint8_t, 1> data = {0};
    EXPECT_TRUE(pack_signal(sig, 66.0, data));
    auto val = extract_signal(sig, data);
    ASSERT_TRUE(val.has_value());
    EXPECT_DOUBLE_EQ(*val, 66.0);
}

TEST(DbcSignal, ZeroBitLength_Nullopt) {
    Signal sig;
    sig.bit_length = 0;
    sig.byte_order = ByteOrder::LittleEndian;
    std::array<uint8_t, 8> data = {};
    EXPECT_FALSE(extract_raw(sig, data).has_value());
}
