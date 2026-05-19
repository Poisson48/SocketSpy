cmake_minimum_required(VERSION 3.28)
project(SocketSpy VERSION 0.8.6 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --------------------------------------------------------------------------- #
# Options
# --------------------------------------------------------------------------- #
option(SOCKETSPY_BUILD_TESTS "Build unit and integration tests"    ON)
option(SOCKETSPY_QML         "Build QML UI (experimental)"         OFF)
option(SOCKETSPY_LUAJIT      "Use LuaJIT instead of Lua 5.4"       OFF)
option(SOCKETSPY_COVERAGE    "Enable coverage instrumentation"      OFF)
option(SOCKETSPY_FUZZ        "Build AFL++ fuzz targets"             OFF)

# --------------------------------------------------------------------------- #
# Dependencies — root only finds what every subdir needs.
# GUI-only (Charts, SerialBus, Widgets) → gui/CMakeLists.txt
# API-only (spdlog, httplib)            → api/CMakeLists.txt
# --------------------------------------------------------------------------- #
find_package(Qt6 REQUIRED COMPONENTS Core)
qt_standard_project_setup()

find_package(PkgConfig REQUIRED)

if(SOCKETSPY_LUAJIT)
    pkg_check_modules(LUA REQUIRED luajit)
else()
    pkg_check_modules(LUA REQUIRED lua5.4)
endif()

find_package(nlohmann_json REQUIRED)

# --------------------------------------------------------------------------- #
# Global compile options
# --------------------------------------------------------------------------- #
add_compile_options(-Wall -Wextra -Wpedantic)
if(SOCKETSPY_COVERAGE)
    add_compile_options(--coverage -O0 -g)
    add_link_options(--coverage)
endif()

# --------------------------------------------------------------------------- #
# Tests — enable_testing() must precede add_subdirectory() so that
# gtest_discover_tests() in subdirs can register test targets.
# --------------------------------------------------------------------------- #
if(SOCKETSPY_BUILD_TESTS)
    include(CTest)
    enable_testing()
endif()

# --------------------------------------------------------------------------- #
# Subdirectories
# --------------------------------------------------------------------------- #
add_subdirectory(core)
add_subdirectory(protocols)
add_subdirectory(dbc)
add_subdirectory(scripting)
add_subdirectory(api)
add_subdirectory(gui)

# --------------------------------------------------------------------------- #
# Install rules
# --------------------------------------------------------------------------- #
include(GNUInstallDirs)
install(FILES ${CMAKE_SOURCE_DIR}/resources/socketspy.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
    OPTIONAL
)
