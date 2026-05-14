// Pure C++ (no Qt) — intentionally no Qt headers included.
// This file accesses socketspy::dbc::Message::signals freely.
#include "dbc_helper.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace socketspy::gui::dbc_helper {

using namespace socketspy::dbc;

std::string decode_frame(const DbcDatabase& db,
                         uint32_t can_id,
                         std::span<const uint8_t> data) {
    for (const auto& msg : db.messages) {
        if (msg.id != can_id) continue;
        std::string result;
        for (const auto& sig : msg.signals) {
            auto val = extract_signal(sig, data);
            if (!val) continue;
            if (!result.empty()) result += "  ";
            result += sig.name + "=";
            // Format value with up to 5 significant figures
            std::ostringstream oss;
            oss << std::setprecision(5) << *val;
            result += oss.str();
            if (!sig.unit.empty()) result += sig.unit;
        }
        return result;
    }
    return {};
}

std::string first_signal_name(const DbcDatabase& db, uint32_t can_id) {
    for (const auto& msg : db.messages) {
        if (msg.id != can_id) continue;
        if (!msg.signals.empty()) return msg.signals[0].name;
    }
    return {};
}

bool signal_range(const DbcDatabase& db,
                  uint32_t msg_id,
                  const std::string& sig_name,
                  double& out_min, double& out_max) {
    for (const auto& msg : db.messages) {
        if (msg.id != msg_id) continue;
        for (const auto& sig : msg.signals) {
            if (sig.name != sig_name) continue;
            out_min = sig.min_val;
            out_max = (sig.max_val > sig.min_val) ? sig.max_val : sig.min_val + 1.0;
            return true;
        }
    }
    return false;
}

std::string signal_unit(const DbcDatabase& db,
                        uint32_t msg_id,
                        const std::string& sig_name) {
    for (const auto& msg : db.messages) {
        if (msg.id != msg_id) continue;
        for (const auto& sig : msg.signals)
            if (sig.name == sig_name) return sig.unit;
    }
    return {};
}

std::optional<double> decode_signal(const DbcDatabase& db,
                                    uint32_t msg_id,
                                    const std::string& sig_name,
                                    std::span<const uint8_t> data) {
    for (const auto& msg : db.messages) {
        if (msg.id != msg_id) continue;
        for (const auto& sig : msg.signals) {
            if (sig.name != sig_name) continue;
            return extract_signal(sig, data);
        }
    }
    return std::nullopt;
}

std::vector<std::string> signal_names_for_msg(const DbcDatabase& db, uint32_t msg_id) {
    for (const auto& msg : db.messages) {
        if (msg.id == msg_id) {
            std::vector<std::string> r;
            r.reserve(msg.signals.size());
            for (const auto& s : msg.signals) r.push_back(s.name);
            return r;
        }
    }
    return {};
}

} // namespace socketspy::gui::dbc_helper
