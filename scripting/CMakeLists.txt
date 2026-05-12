# Agent 5 — Lua scripting subdirectory
# Blocked on: core interface freeze

add_library(scripting STATIC
    src/placeholder.cpp
)

target_include_directories(scripting PUBLIC include)
target_link_libraries(scripting PRIVATE cancore ${LUA_LIBRARIES})
target_include_directories(scripting PRIVATE ${LUA_INCLUDE_DIRS})

if(SOCKETSPY_BUILD_TESTS)
    add_subdirectory(tests)
endif()
