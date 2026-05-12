#include "dbc_parser.h"
#include <cstdint>
#include <string_view>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    std::string_view sv(reinterpret_cast<const char*>(data), size);
    auto result = socketspy::dbc::parse_dbc(sv);
    (void)result;
    return 0;
}
