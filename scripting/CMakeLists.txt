find_package(PkgConfig REQUIRED)
if(SOCKETSPY_LUAJIT)
    pkg_check_modules(LUA REQUIRED luajit)
else()
    pkg_check_modules(LUA REQUIRED lua5.4)
endif()

add_library(scripting STATIC
    src/lua_engine.cpp
    src/lua_sandbox.cpp
    src/lua_can_api.cpp
    src/lua_log_api.cpp
    src/lua_bit_api.cpp
    src/lua_time_api.cpp
)

target_include_directories(scripting
    PUBLIC  include
    PRIVATE src
            ${LUA_INCLUDE_DIRS}
)

target_link_libraries(scripting
    PRIVATE cancore
            ${LUA_LIBRARIES}
)

target_compile_options(scripting PRIVATE
    -Wall -Wextra
    ${LUA_CFLAGS_OTHER}
)

if(SOCKETSPY_BUILD_TESTS)
    add_subdirectory(tests)
endif()
