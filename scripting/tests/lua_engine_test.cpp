#include <chrono>
#include <string>

#include <gtest/gtest.h>

#include "lua_engine.h"

using namespace socketspy::scripting;

// Helper: build a default engine with no watchdog.
static LuaEngine make_engine(uint32_t watchdog_ms = 0) {
    LuaEngine::Config cfg{};
    cfg.watchdog_ms      = watchdog_ms;
    cfg.max_output_bytes = 64 * 1024;
    cfg.on_print         = nullptr;
    return LuaEngine{cfg};
}

// ---------------------------------------------------------------------------
// Basic arithmetic
// ---------------------------------------------------------------------------
TEST(LuaEngine, SimpleArithmetic) {
    auto eng    = make_engine();
    auto result = eng.run("local x = 1 + 1; assert(x == 2)");
    EXPECT_TRUE(result.success) << result.error_message;
}

// ---------------------------------------------------------------------------
// log.print output is captured
// ---------------------------------------------------------------------------
TEST(LuaEngine, LogPrintOutput) {
    std::string captured;
    LuaEngine::Config cfg{};
    cfg.watchdog_ms      = 0;
    cfg.max_output_bytes = 64 * 1024;
    cfg.on_print         = [&](std::string_view msg) { captured = msg; };

    LuaEngine eng{cfg};
    auto result = eng.run("log.print('hello')");
    EXPECT_TRUE(result.success) << result.error_message;
    EXPECT_NE(result.stdout_output.find("hello"), std::string::npos);
    EXPECT_EQ(captured, "hello");
}

// ---------------------------------------------------------------------------
// Script error is captured
// ---------------------------------------------------------------------------
TEST(LuaEngine, ScriptError) {
    auto eng    = make_engine();
    auto result = eng.run("error('oops')");
    EXPECT_FALSE(result.success);
    EXPECT_NE(result.error_message.find("oops"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Watchdog kills infinite loop within 2x watchdog_ms
// ---------------------------------------------------------------------------
TEST(LuaEngine, WatchdogKillsInfiniteLoop) {
    auto eng        = make_engine(100);
    auto t0         = std::chrono::steady_clock::now();
    auto result     = eng.run("while true do end");
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::steady_clock::now() - t0)
                          .count();
    EXPECT_FALSE(result.success);
    EXPECT_LT(elapsed_ms, 500) << "watchdog took too long: " << elapsed_ms << "ms";
}

// ---------------------------------------------------------------------------
// bit.extract
// ---------------------------------------------------------------------------
TEST(LuaEngine, BitExtract) {
    auto eng    = make_engine();
    // bit.extract(0xFF, 0, 4) == 15
    auto result = eng.run(
        "local v = bit.extract(0xFF, 0, 4)\n"
        "assert(v == 15, 'expected 15, got ' .. tostring(v))");
    EXPECT_TRUE(result.success) << result.error_message;
}

// ---------------------------------------------------------------------------
// hex.from_bytes
// ---------------------------------------------------------------------------
TEST(LuaEngine, HexFromBytes) {
    auto eng    = make_engine();
    auto result = eng.run(
        "local s = hex.from_bytes({0xCA, 0xFE})\n"
        "assert(s == 'cafe', 'expected cafe, got ' .. tostring(s))");
    EXPECT_TRUE(result.success) << result.error_message;
}

// ---------------------------------------------------------------------------
// Sandbox: os.execute is nil
// ---------------------------------------------------------------------------
TEST(LuaEngine, SandboxOsExecute) {
    auto eng    = make_engine();
    auto result = eng.run("os.execute('echo hi')");
    EXPECT_FALSE(result.success);
    // Error should mention attempt to call nil
    EXPECT_NE(result.error_message.find("nil"), std::string::npos)
        << "error was: " << result.error_message;
}

// ---------------------------------------------------------------------------
// Sandbox: require is nil
// ---------------------------------------------------------------------------
TEST(LuaEngine, SandboxRequireNil) {
    auto eng    = make_engine();
    auto result = eng.run("require('socket')");
    EXPECT_FALSE(result.success);
    EXPECT_NE(result.error_message.find("nil"), std::string::npos)
        << "error was: " << result.error_message;
}

// ---------------------------------------------------------------------------
// time.now_us returns a positive integer
// ---------------------------------------------------------------------------
TEST(LuaEngine, TimeNowUs) {
    auto eng    = make_engine();
    auto result = eng.run(
        "local t = time.now_us()\n"
        "assert(type(t) == 'number', 'expected number')\n"
        "assert(t > 0, 'expected positive')");
    EXPECT_TRUE(result.success) << result.error_message;
}

// ---------------------------------------------------------------------------
// hex.to_bytes round-trip
// ---------------------------------------------------------------------------
TEST(LuaEngine, HexRoundTrip) {
    auto eng    = make_engine();
    auto result = eng.run(
        "local bytes = hex.to_bytes('deadbeef')\n"
        "assert(#bytes == 4)\n"
        "assert(bytes[1] == 0xDE)\n"
        "assert(bytes[4] == 0xEF)\n"
        "local s = hex.from_bytes(bytes)\n"
        "assert(s == 'deadbeef', s)");
    EXPECT_TRUE(result.success) << result.error_message;
}

// ---------------------------------------------------------------------------
// can.open returns nil (stub)
// ---------------------------------------------------------------------------
TEST(LuaEngine, CanOpenReturnsNil) {
    auto eng    = make_engine();
    auto result = eng.run(
        "local h = can.open('vcan0')\n"
        "assert(h == nil, 'expected nil handle')");
    EXPECT_TRUE(result.success) << result.error_message;
}
