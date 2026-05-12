#include <gtest/gtest.h>
#include "isotp.h"
#include <vector>
#include <cstring>

using namespace socketspy::protocols::isotp;

// ---------------------------------------------------------------------------
// frame_type
// ---------------------------------------------------------------------------

TEST(IsoTp, FrameTypeSingle) {
    const uint8_t d[] = {0x07, 0x01, 0x02};
    EXPECT_EQ(frame_type(d), IsoTpFrameType::Single);
}

TEST(IsoTp, FrameTypeFirst) {
    const uint8_t d[] = {0x10, 0x0A, 0x01};
    EXPECT_EQ(frame_type(d), IsoTpFrameType::First);
}

TEST(IsoTp, FrameTypeConsecutive) {
    const uint8_t d[] = {0x21, 0x01};
    EXPECT_EQ(frame_type(d), IsoTpFrameType::Consecutive);
}

TEST(IsoTp, FrameTypeFlowControl) {
    const uint8_t d[] = {0x30, 0x00, 0x00};
    EXPECT_EQ(frame_type(d), IsoTpFrameType::FlowControl);
}

// ---------------------------------------------------------------------------
// decode_single_frame
// ---------------------------------------------------------------------------

TEST(IsoTp, DecodeSingleFrameBasic) {
    const uint8_t d[] = {0x03, 0xAA, 0xBB, 0xCC, 0x00, 0x00, 0x00, 0x00};
    uint8_t out[7]{};
    uint8_t len = decode_single_frame(d, 8, out, 7);
    EXPECT_EQ(len, 3u);
    EXPECT_EQ(out[0], 0xAAu);
    EXPECT_EQ(out[1], 0xBBu);
    EXPECT_EQ(out[2], 0xCCu);
}

TEST(IsoTp, DecodeSingleFrameMaxPayload) {
    const uint8_t d[] = {0x07, 1, 2, 3, 4, 5, 6, 7};
    uint8_t out[7]{};
    uint8_t len = decode_single_frame(d, 8, out, 7);
    EXPECT_EQ(len, 7u);
    for (uint8_t i = 0; i < 7; ++i) EXPECT_EQ(out[i], i + 1);
}

TEST(IsoTp, DecodeSingleFrameZeroLengthFails) {
    const uint8_t d[] = {0x00, 0x00};
    uint8_t out[7]{};
    EXPECT_EQ(decode_single_frame(d, 2, out, 7), 0u);
}

// ---------------------------------------------------------------------------
// decode_first_frame
// ---------------------------------------------------------------------------

TEST(IsoTp, DecodeFirstFrame) {
    // Total 12 bytes; first frame carries 6 bytes of payload
    const uint8_t d[] = {0x10, 0x0C, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    uint8_t first_payload[6]{};
    uint8_t first_len = 0;
    uint16_t total = decode_first_frame(d, 8, first_payload, first_len);
    EXPECT_EQ(total, 12u);
    EXPECT_EQ(first_len, 6u);
    for (uint8_t i = 0; i < 6; ++i) EXPECT_EQ(first_payload[i], i + 1);
}

// ---------------------------------------------------------------------------
// decode_consecutive_frame
// ---------------------------------------------------------------------------

TEST(IsoTp, DecodeConsecutiveFrame) {
    const uint8_t d[] = {0x21, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D};
    uint8_t sn = 0;
    uint8_t out[7]{};
    uint8_t plen = decode_consecutive_frame(d, 8, sn, out);
    EXPECT_EQ(sn, 1u);
    EXPECT_EQ(plen, 7u);
    EXPECT_EQ(out[0], 0x07u);
}

// ---------------------------------------------------------------------------
// encode_flow_control
// ---------------------------------------------------------------------------

TEST(IsoTp, EncodeFlowControlCTS) {
    uint8_t buf[8]{};
    uint8_t dlc = encode_flow_control(FcFlag::ContinueToSend, 0, 0, buf);
    EXPECT_EQ(dlc, 3u);
    EXPECT_EQ(buf[0], 0x30u);
    EXPECT_EQ(buf[1], 0x00u);
    EXPECT_EQ(buf[2], 0x00u);
}

TEST(IsoTp, EncodeFlowControlWait) {
    uint8_t buf[8]{};
    encode_flow_control(FcFlag::Wait, 5, 10, buf);
    EXPECT_EQ(buf[0], 0x31u);
    EXPECT_EQ(buf[1], 5u);
    EXPECT_EQ(buf[2], 10u);
}

// ---------------------------------------------------------------------------
// IsoTpReceiver — single frame
// ---------------------------------------------------------------------------

TEST(IsoTpReceiver, SingleFrame) {
    std::vector<uint8_t> received;
    IsoTpReceiver rx([&](std::vector<uint8_t> d) { received = std::move(d); });

    const uint8_t frame[] = {0x05, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x00, 0x00};
    EXPECT_TRUE(rx.feed(frame, 8));
    ASSERT_EQ(received.size(), 5u);
    EXPECT_EQ(received[0], 0xAAu);
    EXPECT_EQ(received[4], 0xEEu);
}

// ---------------------------------------------------------------------------
// IsoTpReceiver — multi-frame (12 bytes = FF + 1 CF)
// ---------------------------------------------------------------------------

TEST(IsoTpReceiver, MultiFrame12Bytes) {
    std::vector<uint8_t> received;
    IsoTpReceiver rx([&](std::vector<uint8_t> d) { received = std::move(d); });

    // First frame: total=12, payload bytes 01..06
    const uint8_t ff[] = {0x10, 0x0C, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    EXPECT_TRUE(rx.feed(ff, 8));
    EXPECT_TRUE(received.empty());  // not complete yet

    // Consecutive frame SN=1: payload bytes 07..0C
    const uint8_t cf[] = {0x21, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x00};
    EXPECT_TRUE(rx.feed(cf, 8));
    ASSERT_EQ(received.size(), 12u);
    for (uint8_t i = 0; i < 12; ++i)
        EXPECT_EQ(received[i], static_cast<uint8_t>(i + 1));
}

TEST(IsoTpReceiver, SequenceNumberError) {
    std::vector<uint8_t> received;
    IsoTpReceiver rx([&](std::vector<uint8_t> d) { received = std::move(d); });

    const uint8_t ff[] = {0x10, 0x0C, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    rx.feed(ff, 8);

    // Wrong SN (should be 1, sending 2)
    const uint8_t cf_bad[] = {0x22, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x00};
    EXPECT_FALSE(rx.feed(cf_bad, 8));
    EXPECT_TRUE(received.empty());
}
