find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBURING REQUIRED liburing)

add_library(cancore STATIC
    src/ring_buffer.cpp
    src/can_socket.cpp
)

target_include_directories(cancore
    PUBLIC  include
    PRIVATE src
            ${LIBURING_INCLUDE_DIRS}
)

target_link_libraries(cancore PRIVATE ${LIBURING_LIBRARIES})

target_compile_options(cancore PRIVATE
    -Wall -Wextra -Wpedantic
    ${LIBURING_CFLAGS_OTHER}
)

if(SOCKETSPY_BUILD_TESTS)
    add_subdirectory(tests)
endif()
