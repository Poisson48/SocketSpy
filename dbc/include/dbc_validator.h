#pragma once
#include "dbc_types.h"
#include <vector>
#include <string>

namespace socketspy::dbc {

struct ValidationIssue {
    enum class Severity { Warning, Error };
    Severity    severity;
    uint32_t    message_id;
    std::string signal_name;
    std::string description;
};

// Validate all messages and signals in the database.
// Returns empty vector if valid.
std::vector<ValidationIssue> validate(const DbcDatabase& db) noexcept;

} // namespace socketspy::dbc
