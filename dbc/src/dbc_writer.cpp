#include "dbc_writer.h"
#include <cstdio>
#include <string>

namespace socketspy::dbc {

namespace {

// Format a double with %.6g but strip trailing zeros after decimal point
// to stay compact yet precise enough for round-trip
std::string fmt_double(double v) {
    char buf[64];
    // Use %g which removes trailing zeros; 6 significant digits is the DBC convention
    std::snprintf(buf, sizeof(buf), "%.6g", v);
    return buf;
}

// Escape a string for DBC quoting
std::string dbc_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 4);
    for (char c : s) {
        if (c == '"')  { out += "\\\""; }
        else if (c == '\\') { out += "\\\\"; }
        else { out += c; }
    }
    return out;
}

} // anonymous namespace

std::string write_dbc(const DbcDatabase& db) {
    std::string out;
    out.reserve(4096);

    // VERSION
    out += "VERSION \"";
    out += dbc_escape(db.version);
    out += "\"\n\n";

    // NS_
    out += "NS_ :\n\n";

    // BU_
    out += "BU_:";
    for (const auto& node : db.nodes) {
        out += ' ';
        out += node;
    }
    out += "\n\n";

    // BO_ blocks
    for (const auto& msg : db.messages) {
        uint32_t wire_id = msg.id;
        if (msg.extended) wire_id |= 0x80000000u;

        out += "BO_ ";
        out += std::to_string(wire_id);
        out += ' ';
        out += msg.name;
        out += ": ";
        out += std::to_string(msg.dlc);
        out += ' ';
        out += msg.transmitter;
        out += "\n";

        for (const auto& sig : msg.signals) {
            out += " SG_ ";
            out += sig.name;
            out += " : ";
            out += std::to_string(sig.start_bit);
            out += '|';
            out += std::to_string(sig.bit_length);
            out += '@';
            out += (sig.byte_order == ByteOrder::LittleEndian) ? '1' : '0';
            out += (sig.value_type == ValueType::Unsigned) ? '+' : '-';
            out += " (";
            out += fmt_double(sig.factor);
            out += ',';
            out += fmt_double(sig.offset);
            out += ") [";
            out += fmt_double(sig.min_val);
            out += '|';
            out += fmt_double(sig.max_val);
            out += "] \"";
            out += dbc_escape(sig.unit);
            out += "\" ";
            for (size_t i = 0; i < sig.receivers.size(); ++i) {
                if (i > 0) out += ',';
                out += sig.receivers[i];
            }
            if (sig.receivers.empty()) out += "Vector__XXX";
            out += "\n";
        }
        out += "\n";
    }

    // CM_ blocks — message comments
    for (const auto& msg : db.messages) {
        if (!msg.comment.empty()) {
            out += "CM_ BO_ ";
            out += std::to_string(msg.id | (msg.extended ? 0x80000000u : 0u));
            out += " \"";
            out += dbc_escape(msg.comment);
            out += "\";\n";
        }
    }

    // CM_ SG_ blocks
    for (const auto& msg : db.messages) {
        uint32_t wire_id = msg.id | (msg.extended ? 0x80000000u : 0u);
        for (const auto& sig : msg.signals) {
            for (const auto& [k, v] : sig.attributes) {
                if (k == "comment") {
                    out += "CM_ SG_ ";
                    out += std::to_string(wire_id);
                    out += " \"";
                    out += dbc_escape(sig.name);
                    out += "\" \"";
                    out += dbc_escape(v);
                    out += "\";\n";
                }
            }
        }
    }

    // VAL_ blocks
    for (const auto& msg : db.messages) {
        uint32_t wire_id = msg.id | (msg.extended ? 0x80000000u : 0u);
        for (const auto& sig : msg.signals) {
            if (!sig.value_descriptions.empty()) {
                out += "VAL_ ";
                out += std::to_string(wire_id);
                out += ' ';
                out += sig.name;
                for (const auto& [raw, label] : sig.value_descriptions) {
                    out += ' ';
                    out += std::to_string(raw);
                    out += " \"";
                    out += dbc_escape(label);
                    out += '"';
                }
                out += ";\n";
            }
        }
    }

    // Attribute definitions (raw pass-through)
    for (const auto& attr : db.raw_attribute_defs) {
        out += attr;
        out += '\n';
    }

    // Env vars (raw pass-through)
    for (const auto& ev : db.raw_env_vars) {
        out += ev;
        out += '\n';
    }

    return out;
}

} // namespace socketspy::dbc
