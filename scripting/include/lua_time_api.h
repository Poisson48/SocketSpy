#pragma once

struct lua_State;

namespace socketspy::scripting {

// Register time.sleep_ms(n) and time.now_us() -> integer.
void register_time_api(lua_State* L);

} // namespace socketspy::scripting
