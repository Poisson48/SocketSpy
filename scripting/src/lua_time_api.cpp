#include "lua_time_api.h"

#include <cerrno>
#include <thread>
#include <chrono>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

#include <time.h>

namespace socketspy::scripting {

// ---------------------------------------------------------------------------
// time.sleep_ms(n)
// Sleeps for n milliseconds. Interruptible by the Lua watchdog hook because
// the hook fires on instruction count, not wall-clock time; the sleep is a
// C call so the hook is not invoked during the sleep itself. This is
// acceptable for the demo scripts — production code should use smaller
// increments if precise abort is needed.
// ---------------------------------------------------------------------------
static int time_sleep_ms(lua_State* L) {
    lua_Integer ms = luaL_checkinteger(L, 1);
    if (ms < 0) ms = 0;
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    return 0;
}

// ---------------------------------------------------------------------------
// time.now_us() -> integer
// Returns microseconds since an arbitrary monotonic epoch.
// ---------------------------------------------------------------------------
static int time_now_us(lua_State* L) {
    struct timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    lua_Integer us = static_cast<lua_Integer>(ts.tv_sec) * 1'000'000LL +
                     static_cast<lua_Integer>(ts.tv_nsec) / 1'000LL;
    lua_pushinteger(L, us);
    return 1;
}

static const luaL_Reg kTimeFuncs[] = {
    {"sleep_ms", time_sleep_ms},
    {"now_us",   time_now_us},
    {nullptr,    nullptr},
};

void register_time_api(lua_State* L) {
    lua_newtable(L);
    luaL_setfuncs(L, kTimeFuncs, 0);
    lua_setglobal(L, "time");
}

} // namespace socketspy::scripting
