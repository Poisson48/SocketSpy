add_library(cancore STATIC
    src/ring_buffer.cpp
    src/can_socket.cpp
)

target_include_directories(cancore
    PUBLIC  include
    PRIVATE src
)

target_compile_options(cancore PRIVATE
    -Wall -Wextra -Wpedantic
)

if(SOCKETSPY_BUILD_TESTS)
    add_subdirectory(tests)
endif()
