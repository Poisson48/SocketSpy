# Agent 6 — REST + WebSocket + MCP API

find_package(spdlog REQUIRED)

# cpp-httplib is header-only; libcpp-httplib-dev ships a cmake config on
# Ubuntu 22.04+. Fall back to a plain header search on older distros.
find_package(httplib QUIET)
if(NOT httplib_FOUND)
    find_path(HTTPLIB_INCLUDE_DIR NAMES httplib.h REQUIRED)
    add_library(httplib::httplib INTERFACE IMPORTED)
    target_include_directories(httplib::httplib INTERFACE "${HTTPLIB_INCLUDE_DIR}")
endif()

add_library(apilib STATIC
    src/mcp/mcp_server.cpp
    src/mcp/mcp_server_register.cpp
    src/mcp/tools/can_monitor.cpp
    src/mcp/tools/can_send.cpp
    src/mcp/tools/can_stop.cpp
    src/mcp/tools/can_decode.cpp
    src/mcp/tools/can_replay.cpp
    src/mcp/tools/can_script.cpp
    src/mcp/tools/canopen_sdo_read.cpp
    src/mcp/tools/canopen_sdo_write.cpp
    src/mcp/tools/canopen_scan.cpp
    src/mcp/tools/can_diff.cpp
    src/mcp/tools/get_stats.cpp
)

target_include_directories(apilib
    PUBLIC  include
    PRIVATE src/mcp
            src/mcp/tools
)

target_link_libraries(apilib
    PRIVATE cancore
            nlohmann_json::nlohmann_json
            spdlog::spdlog
            httplib::httplib
)

# Standalone MCP binary
add_executable(socketspy-mcp src/mcp/main.cpp)
target_link_libraries(socketspy-mcp PRIVATE apilib cancore)

if(SOCKETSPY_BUILD_TESTS)
    add_subdirectory(tests)
endif()
