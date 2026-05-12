#include "lua_engine.h"

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#include "lua_bit_api.h"
#include "lua_can_api.h"
#include "lua_log_api.h"
#include "lua_sandbox.h"
#include "lua_time_api.h"

namespace socketspy::scripting {

// ---------------------------------------------------------------------------
// Internal engine state — plain struct (not nested in LuaEngine) so that free
// functions in this TU can access it without friendship.
// ---------------------------------------------------------------------------
struct EngineState {
    LuaEngine::Config     cfg;
    std::atomic<bool>     abort_flag{false};
    std::atomic<bool>     running_flag{false};
    std::string           output_buf;

    explicit EngineState(LuaEngine::Config c) : cfg(std::move(c)) {}
};

// LuaEngine::Impl is an alias so the pimpl header declaration still works.
struct LuaEngine::Impl : EngineState {
    using EngineState::EngineState;
};

// Registry key used to stash EngineState* inside the lua_State.
static const char* kStateKey = "socketspy.engine.state";

// ---------------------------------------------------------------------------
// Debug hook: runs every 1000 VM instructions to check the abort flag.
// Must not touch the lua_State directly (just call lua_error on abort).
// ---------------------------------------------------------------------------
static void abort_hook(lua_State* L, lua_Debug* /*ar*/) {
    lua_pushlightuserdata(L, reinterpret_cast<void*>(
        const_cast<char*>(kStateKey)));
    lua_rawget(L, LUA_REGISTRYINDEX);
    auto* state = static_cast<EngineState*>(lua_touserdata(L, -1));
    lua_pop(L, 1);

    if (state && state->abort_flag.load(std::memory_order_relaxed)) {
        luaL_error(L, "script aborted by watchdog");
    }
}

// ---------------------------------------------------------------------------
// LuaEngine
// ---------------------------------------------------------------------------

LuaEngine::LuaEngine(Config cfg)
    : impl_(std::make_unique<Impl>(std::move(cfg))) {}

LuaEngine::~LuaEngine() = default;

void LuaEngine::abort() noexcept {
    impl_->abort_flag.store(true, std::memory_order_relaxed);
}

bool LuaEngine::running() const noexcept {
    return impl_->running_flag.load(std::memory_order_acquire);
}

ScriptResult LuaEngine::run(std::string_view lua_code) {
    auto t_start = std::chrono::steady_clock::now();

    impl_->abort_flag.store(false, std::memory_order_relaxed);
    impl_->running_flag.store(true, std::memory_order_release);
    impl_->output_buf.clear();

    ScriptResult result{};

    // 1. Create a fresh lua_State.
    lua_State* L = luaL_newstate();
    if (!L) {
        impl_->running_flag.store(false, std::memory_order_release);
        result.success       = false;
        result.error_message = "failed to create Lua state";
        return result;
    }

    // 2. Load standard libs.
    luaL_openlibs(L);

    // 3. Sandbox: remove dangerous APIs.
    lua_sandbox(L);

    // 4. Stash EngineState pointer in Lua registry for the hook.
    lua_pushlightuserdata(L, reinterpret_cast<void*>(
        const_cast<char*>(kStateKey)));
    lua_pushlightuserdata(L, static_cast<EngineState*>(impl_.get()));
    lua_rawset(L, LUA_REGISTRYINDEX);

    // 5. Register API tables.
    register_can_api(L);
    register_protocol_api(L);

    PrintCallback wrapped_print = [this](std::string_view msg) {
        if (impl_->cfg.max_output_bytes == 0 ||
            impl_->output_buf.size() < impl_->cfg.max_output_bytes) {
            impl_->output_buf += msg;
            impl_->output_buf += '\n';
        }
        if (impl_->cfg.on_print) {
            impl_->cfg.on_print(msg);
        }
    };
    register_log_api(L, wrapped_print);
    register_bit_api(L);
    register_time_api(L);

    // 6. Install debug hook: check abort every 1000 VM instructions.
    lua_sethook(L, abort_hook, LUA_MASKCOUNT, 1000);

    // 7. Optional watchdog thread: sets abort_flag after watchdog_ms.
    std::thread watchdog_thread;
    if (impl_->cfg.watchdog_ms > 0) {
        watchdog_thread = std::thread([this, wd_ms = impl_->cfg.watchdog_ms]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(wd_ms));
            if (impl_->running_flag.load(std::memory_order_acquire)) {
                abort();
            }
        });
    }

    // 8. Load and run the script.
    std::string code_str(lua_code);
    int rc = luaL_dostring(L, code_str.c_str());
    if (rc != LUA_OK) {
        result.success = false;
        const char* err = lua_tostring(L, -1);
        result.error_message = err ? err : "(unknown error)";
        lua_pop(L, 1);
    } else {
        result.success = true;
    }

    // 9. Signal the watchdog that we're done and wait for it.
    impl_->running_flag.store(false, std::memory_order_release);
    if (watchdog_thread.joinable()) {
        watchdog_thread.join();
    }

    // 10. Close Lua state (always, even on error).
    lua_close(L);

    auto t_end = std::chrono::steady_clock::now();
    result.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            t_end - t_start)
                            .count();
    result.stdout_output = impl_->output_buf;

    return result;
}

} // namespace socketspy::scripting
