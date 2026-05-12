#pragma once
#include <functional>
#include <string_view>

struct lua_State;

namespace socketspy::scripting {

using PrintCallback = std::function<void(std::string_view)>;

// Register log.print(msg) and log.csv(path, headers_table, row_table).
// log.print() calls the provided callback AND appends to engine output buffer.
// log.csv() writes a CSV row to a local file. No network calls.
void register_log_api(lua_State* L, const PrintCallback& on_print);

} // namespace socketspy::scripting
