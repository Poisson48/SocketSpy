#include <gtest/gtest.h>
#include "cancore.h"
#include <linux/can.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <cstring>

using namespace socketspy::core;

// These tests require vcan0 to be available.
// setup_vcan.sh must be run before executing this suite.
// In CI, vcan0 is set up by the workflow.

class VcanTest : public ::testing::Test {
protected:
    void SetUp() override {
        h_ = can_open("vcan0");
        if (!h_.valid()) {
            GTEST_SKIP() << "vcan0 unavailable — run scripts/dev/setup_vcan.sh";
        }
    }
    void TearDown() override {
        if (h_.valid()) can_close(h_);
    }
    IfaceHandle h_{-1, 0};
};

TEST_F(VcanTest, OpenAndClose) {
    EXPECT_TRUE(h_.valid());
}

TEST_F(VcanTest, SendStandardFrame) {
    CanFrame f{};
    f.id  = 0x123;
    f.dlc = 3;
    f.data[0] = 0xDE; f.data[1] = 0xAD; f.data[2] = 0xBE;
    EXPECT_TRUE(can_send(h_, f));
}

TEST_F(VcanTest, SetFilterPassAll) {
    EXPECT_TRUE(can_set_filters(h_, {}));
}

TEST_F(VcanTest, SetFilterSingleId) {
    CanFilter cf{0x123, 0x7FF};
    EXPECT_TRUE(can_set_filters(h_, {&cf, 1}));
}

TEST_F(VcanTest, SendAndReceiveLoopback) {
    // Open a second socket on the same vcan0 for receive.
    IfaceHandle rx = can_open("vcan0");
    ASSERT_TRUE(rx.valid());

    CanFrame tx{};
    tx.id  = 0x42A;
    tx.dlc = 2;
    tx.data[0] = 0xCA; tx.data[1] = 0xFE;
    EXPECT_TRUE(can_send(h_, tx));

    // Blocking read with 100ms timeout via SO_RCVTIMEO.
    struct timeval tv{0, 100'000};
    ::setsockopt(rx.fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct can_frame raw{};
    ssize_t n = ::read(rx.fd, &raw, sizeof(raw));
    EXPECT_EQ(n, static_cast<ssize_t>(sizeof(raw)));
    EXPECT_EQ(raw.can_id, 0x42Au);
    EXPECT_EQ(raw.data[0], 0xCAu);
    EXPECT_EQ(raw.data[1], 0xFEu);

    can_close(rx);
}

TEST_F(VcanTest, FdModeToggle) {
    EXPECT_TRUE(can_set_fd_mode(h_, true));
    EXPECT_TRUE(can_set_fd_mode(h_, false));
}

TEST_F(VcanTest, ClassifyErrorFrameNone) {
    CanFrame f{};
    EXPECT_EQ(classify_error(f), ErrorType::None);
}

TEST_F(VcanTest, InvalidIfaceReturnsInvalid) {
    auto bad = can_open("nonexistent99");
    EXPECT_FALSE(bad.valid());
}
