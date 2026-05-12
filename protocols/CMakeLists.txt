# Agent 2 — protocols subdirectory
# Blocked on: core interface freeze
# Status: scaffold only

add_library(protocols STATIC
    canopen/placeholder.cpp
)

target_include_directories(protocols
    PUBLIC  canopen/include
            j1939/include
            uds/include
            isotp/include
            obd2/include
            nmea2000/include
)

target_link_libraries(protocols PRIVATE cancore)

if(SOCKETSPY_BUILD_TESTS)
    add_subdirectory(tests)
endif()
