# Agent 4 — Qt 6.6+ GUI

find_package(Qt6 REQUIRED COMPONENTS Widgets Charts SerialBus SerialPort Bluetooth LinguistTools)
find_package(OpenSSL REQUIRED COMPONENTS Crypto)

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
    include/sim_profile_editor.h
    include/scripting_panel.h
    include/protocol_panel.h
    include/can_iface_config.h
    include/project.h
    include/project_registry.h
    include/project_browser.h
    include/dbc_builder_panel.h
    include/welcome_screen.h
    include/permission_checker.h
    include/mcp_panel.h
    include/fuzzer_panel.h
    include/diff_panel.h
    include/uds_panel.h
    include/uds_transport.h
    include/uds_scanner.h
    include/uds_security.h
    include/blf_writer.h
    include/mdf4_writer.h
    include/asc_io.h
    include/trc_io.h
    include/pcap_io.h
    include/temporal_panel.h
    include/heatmap_panel.h
    include/heatmap_widget.h
    include/bisect_panel.h
    include/range_state_panel.h
    include/range_state_scan.h
    include/signal_detective_panel.h
    include/monitor_delegate.h
    include/xcp_panel.h
    include/doip_panel.h
    include/opendbc_panel.h
    include/sidebar_nav.h
    include/uds_ecu_sim.h
    include/splash_screen.h
    include/updater.h
    include/update_dialog.h
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
    src/sim_profile_editor.cpp
    src/scripting/scripting_panel.cpp
    src/protocol_panel.cpp
    src/can_iface_config.cpp
    src/project.cpp
    src/project_registry.cpp
    src/project_browser.cpp
    src/dbc_builder_panel.cpp
    src/welcome_screen.cpp
    src/permission_checker.cpp
    src/mcp_panel.cpp
    src/fuzzer_panel.cpp
    src/diff_panel.cpp
    src/uds_panel.cpp
    src/uds_transport.cpp
    src/uds_scanner.cpp
    src/uds_security.cpp
    src/blf_writer.cpp
    src/mdf4_writer.cpp
    src/asc_io.cpp
    src/trc_io.cpp
    src/pcap_io.cpp
    src/temporal_panel.cpp
    src/heatmap_panel.cpp
    src/heatmap_widget.cpp
    src/bisect_panel.cpp
    src/range_state_panel.cpp
    src/range_state_scan.cpp
    src/signal_detective_panel.cpp
    src/monitor_delegate.cpp
    src/xcp_panel.cpp
    src/doip_panel.cpp
    src/opendbc_panel.cpp
    src/sidebar_nav.cpp
    src/uds_ecu_sim.cpp
    src/splash_screen.cpp
    src/updater.cpp
    src/update_dialog.cpp
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

target_compile_definitions(socketspy PRIVATE
    APP_VERSION="${PROJECT_VERSION}"
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
            Qt6::Bluetooth
            Qt6::Network
            nlohmann_json::nlohmann_json
            OpenSSL::Crypto
)

install(TARGETS socketspy RUNTIME DESTINATION bin)

# ---------------------------------------------------------------------------
# Internationalization (i18n)
# ---------------------------------------------------------------------------
# qt_add_translations() generates .qm files from the .ts stubs and embeds
# them into the socketspy binary as Qt resources under :/i18n/.
# Run lupdate against the socketspy target to refresh the .ts files whenever
# new tr()-wrapped strings are added.
qt_add_translations(socketspy
    TS_FILES
        translations/socketspy_en.ts
        translations/socketspy_fr.ts
    RESOURCE_PREFIX "/i18n"
    QM_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/translations"
)

if(SOCKETSPY_BUILD_TESTS)
    add_subdirectory(tests)
endif()
