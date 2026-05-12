#include <gtest/gtest.h>
#include "canopen_nmt.h"

using namespace socketspy::protocols::canopen;

// ---------------------------------------------------------------------------
// decode_nmt
// ---------------------------------------------------------------------------

TEST(CanopenNmt, DecodeNmtEnterOperational) {
    const uint8_t data[] = {0x01, 0x00};
    NmtMessage msg{};
    ASSERT_TRUE(decode_nmt(0x000, data, 2, msg));
    EXPECT_EQ(msg.command, NmtCommand::EnterOperational);
    EXPECT_EQ(msg.node_id, 0u);
}

TEST(CanopenNmt, DecodeNmtEnterStop) {
    const uint8_t data[] = {0x02, 0x07};
    NmtMessage msg{};
    ASSERT_TRUE(decode_nmt(0x000, data, 2, msg));
    EXPECT_EQ(msg.command, NmtCommand::EnterStop);
    EXPECT_EQ(msg.node_id, 7u);
}

TEST(CanopenNmt, DecodeNmtEnterPreOperational) {
    const uint8_t data[] = {0x80, 0x1F};
    NmtMessage msg{};
    ASSERT_TRUE(decode_nmt(0x000, data, 2, msg));
    EXPECT_EQ(msg.command, NmtCommand::EnterPreOperational);
    EXPECT_EQ(msg.node_id, 0x1F);
}

TEST(CanopenNmt, DecodeNmtResetNode) {
    const uint8_t data[] = {0x81, 0x03};
    NmtMessage msg{};
    ASSERT_TRUE(decode_nmt(0x000, data, 2, msg));
    EXPECT_EQ(msg.command, NmtCommand::ResetNode);
    EXPECT_EQ(msg.node_id, 3u);
}

TEST(CanopenNmt, DecodeNmtResetCommunication) {
    const uint8_t data[] = {0x82, 0x00};
    NmtMessage msg{};
    ASSERT_TRUE(decode_nmt(0x000, data, 2, msg));
    EXPECT_EQ(msg.command, NmtCommand::ResetCommunication);
    EXPECT_EQ(msg.node_id, 0u);
}

TEST(CanopenNmt, DecodeNmtWrongCanId) {
    const uint8_t data[] = {0x01, 0x00};
    NmtMessage msg{};
    EXPECT_FALSE(decode_nmt(0x001, data, 2, msg));
    EXPECT_FALSE(decode_nmt(0x700, data, 2, msg));
}

TEST(CanopenNmt, DecodeNmtDlcTooShort) {
    const uint8_t data[] = {0x01};
    NmtMessage msg{};
    EXPECT_FALSE(decode_nmt(0x000, data, 1, msg));
}

// ---------------------------------------------------------------------------
// encode_nmt
// ---------------------------------------------------------------------------

TEST(CanopenNmt, EncodeNmtRoundTrip) {
    NmtMessage orig{NmtCommand::EnterPreOperational, 0x42};
    uint8_t buf[2]{};
    uint8_t dlc = encode_nmt(orig, buf);
    EXPECT_EQ(dlc, 2u);
    EXPECT_EQ(buf[0], 0x80u);
    EXPECT_EQ(buf[1], 0x42u);

    NmtMessage decoded{};
    ASSERT_TRUE(decode_nmt(0x000, buf, dlc, decoded));
    EXPECT_EQ(decoded.command, orig.command);
    EXPECT_EQ(decoded.node_id, orig.node_id);
}

TEST(CanopenNmt, EncodeNmtBroadcast) {
    NmtMessage msg{NmtCommand::EnterOperational, 0x00};
    uint8_t buf[2]{};
    encode_nmt(msg, buf);
    EXPECT_EQ(buf[0], 0x01u);
    EXPECT_EQ(buf[1], 0x00u);
}

// ---------------------------------------------------------------------------
// decode_heartbeat
// ---------------------------------------------------------------------------

TEST(CanopenNmt, HeartbeatOperational) {
    const uint8_t data[] = {0x05};
    uint8_t node_id = 0;
    NmtState state{};
    ASSERT_TRUE(decode_heartbeat(0x705, data, 1, node_id, state));
    EXPECT_EQ(node_id, 5u);
    EXPECT_EQ(state, NmtState::Operational);
}

TEST(CanopenNmt, HeartbeatPreOperational) {
    const uint8_t data[] = {0x7F};
    uint8_t node_id = 0;
    NmtState state{};
    ASSERT_TRUE(decode_heartbeat(0x710, data, 1, node_id, state));
    EXPECT_EQ(node_id, 0x10u);
    EXPECT_EQ(state, NmtState::PreOperational);
}

TEST(CanopenNmt, HeartbeatBootUp) {
    const uint8_t data[] = {0x00};  // 0x00 = Initialising
    uint8_t node_id = 0;
    NmtState state{};
    ASSERT_TRUE(decode_heartbeat(0x701, data, 1, node_id, state));
    EXPECT_EQ(node_id, 1u);
    EXPECT_EQ(state, NmtState::Initialising);
}

TEST(CanopenNmt, HeartbeatStopped) {
    const uint8_t data[] = {0x04};
    uint8_t node_id = 0;
    NmtState state{};
    ASSERT_TRUE(decode_heartbeat(0x77F, data, 1, node_id, state));
    EXPECT_EQ(node_id, 0x7Fu);
    EXPECT_EQ(state, NmtState::Stopped);
}

TEST(CanopenNmt, HeartbeatWrongRange) {
    const uint8_t data[] = {0x05};
    uint8_t node_id = 0;
    NmtState state{};
    // 0x700 has node_id==0, which we reject
    EXPECT_FALSE(decode_heartbeat(0x700, data, 1, node_id, state));
    EXPECT_FALSE(decode_heartbeat(0x780, data, 1, node_id, state));
}

// ---------------------------------------------------------------------------
// String helpers
// ---------------------------------------------------------------------------

TEST(CanopenNmt, StateNames) {
    EXPECT_EQ(nmt_state_name(NmtState::Operational),    "Operational");
    EXPECT_EQ(nmt_state_name(NmtState::Stopped),        "Stopped");
    EXPECT_EQ(nmt_state_name(NmtState::Initialising),   "Initialising");
    EXPECT_EQ(nmt_state_name(NmtState::PreOperational), "PreOperational");
    EXPECT_EQ(nmt_state_name(NmtState::Unknown),        "Unknown");
}

TEST(CanopenNmt, CommandNames) {
    EXPECT_EQ(nmt_command_name(NmtCommand::EnterOperational),    "EnterOperational");
    EXPECT_EQ(nmt_command_name(NmtCommand::EnterStop),           "EnterStop");
    EXPECT_EQ(nmt_command_name(NmtCommand::EnterPreOperational), "EnterPreOperational");
    EXPECT_EQ(nmt_command_name(NmtCommand::ResetNode),           "ResetNode");
    EXPECT_EQ(nmt_command_name(NmtCommand::ResetCommunication),  "ResetCommunication");
}
