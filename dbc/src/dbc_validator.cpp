#include "dbc_validator.h"
#include <algorithm>
#include <bitset>
#include <cstdint>

namespace socketspy::dbc {

namespace {

using Issue = ValidationIssue;
using Sev   = ValidationIssue::Severity;

// Build a bitmask of the bits occupied by a signal (LE or BE)
// Returns false if the signal has invalid parameters
static bool signal_bit_mask(const Signal& sig, std::bitset<64>& mask) noexcept {
    const uint32_t len = sig.bit_length;
    if (len == 0 || len > 64u) return false;
    mask.reset();

    if (sig.byte_order == ByteOrder::LittleEndian) {
        uint32_t start = sig.start_bit;
        if (start + len > 64u) return false;
        for (uint32_t b = 0; b < len; ++b)
            mask.set(start + b);
    } else {
        // Big-endian: walk the same chain as extraction
        uint32_t bit_pos = sig.start_bit;
        for (uint32_t i = 0; i < len; ++i) {
            uint32_t byte_idx = bit_pos / 8u;
            uint32_t bit_idx  = bit_pos % 8u;
            uint32_t flat = byte_idx * 8u + bit_idx;
            if (flat >= 64u) return false;
            mask.set(flat);
            if (bit_idx == 0) bit_pos = (byte_idx + 1u) * 8u + 7u;
            else --bit_pos;
        }
    }
    return true;
}

} // anonymous namespace

std::vector<ValidationIssue> validate(const DbcDatabase& db) noexcept {
    std::vector<ValidationIssue> issues;

    for (const auto& msg : db.messages) {
        // Per-message bit occupation tracker (64 bits = 8 bytes max CAN)
        std::bitset<64> occupied;
        occupied.reset();

        for (const auto& sig : msg.signals) {
            // Check bit_length > 0
            if (sig.bit_length == 0) {
                issues.push_back({Sev::Error, msg.id, sig.name,
                    "signal has zero bit length"});
                continue;
            }

            // LE bounds check
            if (sig.byte_order == ByteOrder::LittleEndian) {
                uint32_t end_bit = static_cast<uint32_t>(sig.start_bit) +
                                   static_cast<uint32_t>(sig.bit_length);
                if (end_bit > 64u) {
                    issues.push_back({Sev::Error, msg.id, sig.name,
                        "signal extends beyond 64 bits (little-endian)"});
                    continue;
                }
            }

            // Build bit mask
            std::bitset<64> sig_mask;
            if (!signal_bit_mask(sig, sig_mask)) {
                issues.push_back({Sev::Error, msg.id, sig.name,
                    "signal bit position out of range"});
                continue;
            }

            // Overlap check
            if ((occupied & sig_mask).any()) {
                issues.push_back({Sev::Error, msg.id, sig.name,
                    "signal overlaps with another signal in the same message"});
            }
            occupied |= sig_mask;

            // Factor == 0 warning
            if (sig.factor == 0.0) {
                issues.push_back({Sev::Warning, msg.id, sig.name,
                    "signal factor is zero — physical value will always be offset"});
            }

            // min > max warning
            if (sig.min_val > sig.max_val) {
                issues.push_back({Sev::Warning, msg.id, sig.name,
                    "signal min_val is greater than max_val"});
            }
        }
    }

    return issues;
}

} // namespace socketspy::dbc
