#include <gtest/gtest.h>
#include "cancore.h"
#include <thread>
#include <atomic>

using socketspy::core::CanFrame;
using socketspy::core::RingBuffer;

static CanFrame make_frame(uint32_t id, uint64_t ts = 0) {
    CanFrame f{};
    f.id           = id;
    f.timestamp_us = ts;
    f.dlc          = 8;
    return f;
}

TEST(RingBuffer, ConstructWithPowerOfTwo) {
    RingBuffer rb(64);
    EXPECT_EQ(rb.capacity(), 64u);
    EXPECT_TRUE(rb.empty());
    EXPECT_FALSE(rb.full());
}

TEST(RingBuffer, ConstructRoundsUpToPowerOfTwo) {
    RingBuffer rb(100);
    EXPECT_EQ(rb.capacity(), 128u);
}

TEST(RingBuffer, PushPopSingleFrame) {
    RingBuffer rb(16);
    auto in = make_frame(0x123, 999);
    EXPECT_TRUE(rb.push(in));

    CanFrame out{};
    EXPECT_TRUE(rb.pop(out));
    EXPECT_EQ(out.id, 0x123u);
    EXPECT_EQ(out.timestamp_us, 999u);
    EXPECT_TRUE(rb.empty());
}

TEST(RingBuffer, FillAndDrainExact) {
    RingBuffer rb(8);
    for (uint32_t i = 0; i < 8; ++i) EXPECT_TRUE(rb.push(make_frame(i)));
    EXPECT_TRUE(rb.full());

    // One more push must fail (buffer full)
    EXPECT_FALSE(rb.push(make_frame(99)));

    for (uint32_t i = 0; i < 8; ++i) {
        CanFrame f{};
        EXPECT_TRUE(rb.pop(f));
        EXPECT_EQ(f.id, i);
    }
    EXPECT_TRUE(rb.empty());
    EXPECT_FALSE(rb.full());
}

TEST(RingBuffer, PopOnEmptyReturnsFalse) {
    RingBuffer rb(4);
    CanFrame f{};
    EXPECT_FALSE(rb.pop(f));
}

TEST(RingBuffer, WrapsAroundCorrectly) {
    RingBuffer rb(4);
    // Fill, drain half, fill again — exercises index wrap.
    for (int i = 0; i < 4; ++i) rb.push(make_frame(i));
    CanFrame f{};
    rb.pop(f); rb.pop(f);
    for (int i = 10; i < 12; ++i) EXPECT_TRUE(rb.push(make_frame(i)));
    for (int i = 2; i < 4; ++i) { rb.pop(f); EXPECT_EQ(f.id, (uint32_t)i); }
    for (int i = 10; i < 12; ++i) { rb.pop(f); EXPECT_EQ(f.id, (uint32_t)i); }
    EXPECT_TRUE(rb.empty());
}

// Single-producer single-consumer stress test
TEST(RingBuffer, SPSCStress) {
    constexpr size_t kCapacity  = 1024;
    constexpr size_t kNumFrames = 100'000;
    RingBuffer rb(kCapacity);

    std::atomic<bool> done{false};
    size_t consumed = 0;

    std::thread consumer([&] {
        CanFrame f{};
        while (!done.load(std::memory_order_acquire) || !rb.empty()) {
            if (rb.pop(f)) ++consumed;
        }
    });

    for (size_t i = 0; i < kNumFrames; ++i) {
        while (!rb.push(make_frame(i % 0x7FF, i))) {
            // spin: consumer is slow
        }
    }
    done.store(true, std::memory_order_release);
    consumer.join();

    EXPECT_EQ(consumed, kNumFrames);
}
