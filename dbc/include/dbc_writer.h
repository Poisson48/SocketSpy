#pragma once
#include "dbc_types.h"
#include <string>

namespace socketspy::dbc {

// Serialize a DbcDatabase back to DBC text.
// Round-trip guarantee: parse_dbc(write_dbc(db)) == db for all valid inputs.
std::string write_dbc(const DbcDatabase& db);

} // namespace socketspy::dbc
