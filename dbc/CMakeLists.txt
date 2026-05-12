add_library(dbclib STATIC
    src/dbc_types.cpp
    src/dbc_parser.cpp
    src/dbc_writer.cpp
    src/dbc_signal.cpp
    src/dbc_validator.cpp
)

target_include_directories(dbclib PUBLIC include)
target_compile_options(dbclib PRIVATE -Wall -Wextra -Wpedantic)

if(SOCKETSPY_BUILD_TESTS)
    add_subdirectory(tests)
endif()
