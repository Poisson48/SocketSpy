find_package(GTest QUIET)
if(NOT GTest_FOUND)
    message(WARNING "GTest not found — skipping protocol tests")
    return()
endif()

add_executable(protocols-test
    canopen_nmt_test.cpp
    j1939_frame_test.cpp
    isotp_test.cpp
)

target_link_libraries(protocols-test
    PRIVATE protocols
            GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(protocols-test)
