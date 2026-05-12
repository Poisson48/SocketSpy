add_library(protocols STATIC
    canopen/src/canopen_nmt.cpp
    canopen/src/canopen_sdo.cpp
    canopen/src/canopen_pdo.cpp
    canopen/src/canopen_emcy.cpp
    canopen/src/canopen_heartbeat.cpp
    j1939/src/j1939_frame.cpp
    j1939/src/j1939_transport.cpp
    j1939/src/j1939_diag.cpp
    isotp/src/isotp.cpp
    uds/src/uds_client.cpp
    obd2/src/obd2.cpp
    nmea2000/src/nmea2000.cpp
)

target_include_directories(protocols
    PUBLIC
        canopen/include
        j1939/include
        isotp/include
        uds/include
        obd2/include
        nmea2000/include
    PRIVATE
        canopen/src
)

target_link_libraries(protocols PRIVATE cancore)

if(SOCKETSPY_BUILD_TESTS)
    add_subdirectory(tests)
endif()
