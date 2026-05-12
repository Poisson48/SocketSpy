if(NOT SOCKETSPY_FUZZ)
    return()
endif()

add_executable(fuzz_dbc_parser fuzz_dbc_parser.cpp)
target_include_directories(fuzz_dbc_parser PRIVATE ../../include)
target_link_libraries(fuzz_dbc_parser PRIVATE dbclib -fsanitize=fuzzer)
target_compile_options(fuzz_dbc_parser PRIVATE -fsanitize=fuzzer)

add_executable(fuzz_eds_parser fuzz_eds_parser.cpp)
target_link_libraries(fuzz_eds_parser PRIVATE -fsanitize=fuzzer)
target_compile_options(fuzz_eds_parser PRIVATE -fsanitize=fuzzer)
