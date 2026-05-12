# Agent 4 — Qt 6.6+ GUI

find_package(Qt6 REQUIRED COMPONENTS Widgets Charts SerialBus SerialPort)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(GUI_HEADERS
    include/can_capture.h
    include/main_window.h
    include/iface_detector.h
    include/monitor_panel.h
    include/transmit_panel.h
    include/signal_graph.h
    include/log_recorder.h
    include/replay_panel.h
    include/filter_model.h
    include/filter_panel.h
    include/stats_panel.h
    include/trigger_config.h
    include/trigger_panel.h
    include/elm327_bridge.h
    include/elm327_panel.h
    include/sim_profile.h
    include/can_simulator.h
    include/simulator_panel.h
)

add_executable(socketspy
    src/main.cpp
    src/main_window.cpp
    src/main_window_actions.cpp
    src/iface_detector.cpp
    src/can_capture.cpp
    src/monitor_panel.cpp
    src/transmit_panel.cpp
    src/signal_graph.cpp
    src/stats_panel.cpp
    src/dbc_helper.cpp
    src/log_recorder.cpp
    src/replay_panel.cpp
    src/filter_model.cpp
    src/filter_panel.cpp
    src/trigger_panel.cpp
    src/elm327_bridge.cpp
    src/elm327_panel.cpp
    src/sim_profile.cpp
    src/can_simulator.cpp
    src/simulator_panel.cpp
    resources/resources.qrc
    ${GUI_HEADERS}
)

target_include_directories(socketspy
    PRIVATE include
            ../core/include
            ../dbc/include
            ../protocols/include
            ../scripting/include
            ../api/src/mcp
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
            Qt6::SerialPort
            nlohmann_json::nlohmann_json
)

install(TARGETS socketspy RUNTIME DESTINATION bin)

if(SOCKETSPY_BUILD_TESTS)
    add_subdirectory(tests)
endif()
