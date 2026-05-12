# Agent 4 — Qt 6.6+ GUI

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

add_executable(socketspy
    src/main.cpp
)

target_include_directories(socketspy
    PRIVATE include
            src/monitor
            src/graphs
            src/transmit
            src/replay
            src/editor
            src/re
            src/scripting
            src/settings
)

target_link_libraries(socketspy
    PRIVATE cancore
            protocols
            dbclib
            scripting
            apilib
            Qt6::Core
            Qt6::Widgets
            Qt6::Charts
            Qt6::SerialBus
)

install(TARGETS socketspy RUNTIME DESTINATION bin)

if(SOCKETSPY_BUILD_TESTS)
    add_subdirectory(tests)
endif()
