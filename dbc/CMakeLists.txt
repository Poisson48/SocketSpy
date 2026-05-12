# Agent 3 — DBC parser subdirectory
# Status: scaffold only

add_library(dbclib STATIC
    src/placeholder.cpp
)

target_include_directories(dbclib PUBLIC include)
target_link_libraries(dbclib PRIVATE cancore)

if(SOCKETSPY_BUILD_TESTS)
    add_subdirectory(tests)
endif()
