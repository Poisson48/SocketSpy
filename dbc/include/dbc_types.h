#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>

namespace socketspy::dbc {

enum class ByteOrder : uint8_t {
    LittleEndian = 1,  // Intel / Motorola LSB
    BigEndian    = 0,  // Motorola MSB
};

enum class ValueType : uint8_t {
    Unsigned = '+',
    Signed   = '-',
};

struct Signal {
    std::string name;
    uint8_t     start_bit{0};
    uint8_t     bit_length{0};
    ByteOrder   byte_order{ByteOrder::LittleEndian};
    ValueType   value_type{ValueType::Unsigned};
    double      factor{1.0};
    double      offset{0.0};
    double      min_val{0.0};
    double      max_val{0.0};
    std::string unit;
    std::vector<std::string> receivers;
    // Optional value descriptions: raw_value -> label
    std::unordered_map<int64_t, std::string> value_descriptions;
    // Optional attributes (pass-through for lossless round-trip)
    std::vector<std::pair<std::string, std::string>> attributes;
};

struct Message {
    uint32_t    id{0};        // CAN ID (without EFF/RTR flags)
    bool        extended{false};  // 29-bit extended ID
    std::string name;
    uint8_t     dlc{0};
    std::string transmitter;
    std::vector<Signal> signals;
    std::vector<std::pair<std::string, std::string>> attributes;
    std::string comment;
};

struct DbcDatabase {
    std::string              version;
    std::vector<std::string> nodes;
    std::vector<Message>     messages;
    // Global attributes for round-trip
    std::vector<std::string> raw_attribute_defs;
    std::vector<std::string> raw_env_vars;
};

} // namespace socketspy::dbc
