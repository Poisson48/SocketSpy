find_package(GTest QUIET)
if(NOT GTest_FOUND)
    message(WARNING "GTest not found — skipping dbc tests")
    return()
endif()

add_executable(dbc-test
    dbc_parser_test.cpp
    dbc_signal_test.cpp
    dbc_roundtrip_test.cpp
)

target_link_libraries(dbc-test PRIVATE dbclib GTest::gtest_main)
target_compile_options(dbc-test PRIVATE -Wall -Wextra)

include(GoogleTest)
gtest_discover_tests(dbc-test)

if(SOCKETSPY_FUZZ)
    add_subdirectory(fuzz)
endif()
