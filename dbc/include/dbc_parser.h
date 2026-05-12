#pragma once
#include "dbc_types.h"
#include <string_view>
#include <expected>    // C++23
#include <string>

namespace socketspy::dbc {

enum class ParseError {
    UnexpectedEof,
    UnexpectedToken,
    InvalidNumber,
    DuplicateMessage,
    InvalidSignalBit,
    FileTooLarge,
};

std::string_view parse_error_string(ParseError e) noexcept;

// Parse a DBC file from a string buffer.
// Returns a populated DbcDatabase or a ParseError.
// NEVER crashes on malformed input (fuzz-safe).
std::expected<DbcDatabase, ParseError>
parse_dbc(std::string_view input) noexcept;

} // namespace socketspy::dbc
