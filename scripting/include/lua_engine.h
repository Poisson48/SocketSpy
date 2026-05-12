#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace socketspy::scripting {

// Result of a script run
struct ScriptResult {
    bool        success;
    std::string stdout_output;
    std::string error_message;
    int64_t     elapsed_ms;
};

// Output callback: called each time a script calls log.print()
using PrintCallback = std::function<void(std::string_view)>;

class LuaEngine {
public:
    struct Config {
        uint32_t      watchdog_ms;      // kill script after this many ms (0=disabled)
        size_t        max_output_bytes; // truncate stdout beyond this
        PrintCallback on_print;         // called for log.print(); may be null
    };

    explicit LuaEngine(Config cfg);
    ~LuaEngine();

    // Run a Lua script string. Blocks until complete or watchdog fires.
    ScriptResult run(std::string_view lua_code);

    // Abort a running script (called from another thread).
    void abort() noexcept;

    // True if a script is currently running.
    bool running() const noexcept;

    LuaEngine(const LuaEngine&)            = delete;
    LuaEngine& operator=(const LuaEngine&) = delete;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace socketspy::scripting
