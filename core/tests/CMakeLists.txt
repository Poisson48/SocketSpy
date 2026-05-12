find_package(GTest QUIET)

if(NOT GTest_FOUND)
    message(WARNING "GTest not found — skipping cancore-test. Install libgtest-dev.")
    return()
endif()

add_executable(cancore-test
    ring_buffer_test.cpp
    can_socket_test.cpp
)

target_link_libraries(cancore-test
    PRIVATE cancore
            GTest::gtest_main
)

target_compile_options(cancore-test PRIVATE -Wall -Wextra)

include(GoogleTest)
gtest_discover_tests(cancore-test)
