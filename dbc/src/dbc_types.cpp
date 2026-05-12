#include "dbc_parser.h"
#include "dbc_types.h"
#include <string_view>

namespace socketspy::dbc {

std::string_view parse_error_string(ParseError e) noexcept {
    switch (e) {
        case ParseError::UnexpectedEof:    return "unexpected end of file";
        case ParseError::UnexpectedToken:  return "unexpected token";
        case ParseError::InvalidNumber:    return "invalid number literal";
        case ParseError::DuplicateMessage: return "duplicate message ID";
        case ParseError::InvalidSignalBit: return "invalid signal bit position";
        case ParseError::FileTooLarge:     return "file too large";
    }
    return "unknown error";
}

std::string_view byte_order_string(ByteOrder bo) noexcept {
    return (bo == ByteOrder::LittleEndian) ? "little_endian" : "big_endian";
}

std::string_view value_type_string(ValueType vt) noexcept {
    return (vt == ValueType::Unsigned) ? "unsigned" : "signed";
}

} // namespace socketspy::dbc
