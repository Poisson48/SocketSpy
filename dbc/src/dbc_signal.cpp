#include "dbc_signal.h"
#include <cmath>
#include <cstdint>
#include <limits>

namespace socketspy::dbc {

// ---------------------------------------------------------------------------
// Little-endian (Intel) extraction
// start_bit: LSB position in the flat bit array
//   byte[0] bit 0 = position 0 ... byte[0] bit 7 = position 7
//   byte[1] bit 0 = position 8 ... etc.
// ---------------------------------------------------------------------------
static std::optional<int64_t>
extract_le(const Signal& sig, std::span<const uint8_t> data) noexcept {
    const uint32_t start = sig.start_bit;
    const uint32_t len   = sig.bit_length;
    // Need bytes [start/8 .. (start+len-1)/8]
    const uint32_t last_bit  = start + len - 1u;
    const uint32_t last_byte = last_bit / 8u;
    if (last_byte >= data.size()) return std::nullopt;

    uint64_t raw = 0;
    for (uint32_t bit = 0; bit < len; ++bit) {
        uint32_t src_bit  = start + bit;
        uint32_t byte_idx = src_bit / 8u;
        uint32_t bit_idx  = src_bit % 8u;
        if ((data[byte_idx] >> bit_idx) & 1u)
            raw |= (uint64_t{1} << bit);
    }

    // Sign-extend if signed
    if (sig.value_type == ValueType::Signed && len < 64u) {
        uint64_t sign_bit = uint64_t{1} << (len - 1u);
        if (raw & sign_bit) raw |= ~((sign_bit << 1u) - 1u);
    }
    return static_cast<int64_t>(raw);
}

// ---------------------------------------------------------------------------
// Big-endian (Motorola MSB) extraction
// DBC convention: start_bit is the MSB position using the bit numbering:
//   byte 0 = bits 7..0, byte 1 = bits 15..8, etc.
//
// To convert DBC big-endian bit number to [byte, bit-in-byte]:
//   byte_idx = bit_num / 8
//   bit_in_byte = bit_num % 8  (bit 7 = MSB of that byte)
//
// Starting from the MSB bit, we walk bit_length bits following the
// Motorola layout: within a byte, bits descend 7..0; when we reach bit 0
// of a byte, we jump to bit 7 of the next byte.
// ---------------------------------------------------------------------------
static std::optional<int64_t>
extract_be(const Signal& sig, std::span<const uint8_t> data) noexcept {
    const uint32_t len = sig.bit_length;
    uint32_t cur_bit = sig.start_bit; // MSB in DBC numbering

    // Safety: check that all bytes we'll touch are in range
    // The furthest byte we can touch: follow the chain
    {
        uint32_t tmp = cur_bit;
        for (uint32_t i = 0; i < len; ++i) {
            uint32_t b = tmp / 8u;
            if (b >= data.size()) return std::nullopt;
            uint32_t bi = tmp % 8u;
            if (bi == 0) tmp = (b + 1) * 8u + 7u;
            else --tmp;
        }
    }

    uint64_t raw = 0;
    uint32_t bit_pos = cur_bit;
    for (uint32_t i = 0; i < len; ++i) {
        uint32_t byte_idx = bit_pos / 8u;
        uint32_t bit_idx  = bit_pos % 8u;
        uint64_t bit_val  = (data[byte_idx] >> bit_idx) & 1u;
        // MSB is placed at highest position in raw
        raw |= (bit_val << (len - 1u - i));
        // Advance: decrement bit_in_byte; wrap to next byte at bit 7
        if (bit_idx == 0) {
            bit_pos = (byte_idx + 1u) * 8u + 7u;
        } else {
            --bit_pos;
        }
    }

    if (sig.value_type == ValueType::Signed && len < 64u) {
        uint64_t sign_bit = uint64_t{1} << (len - 1u);
        if (raw & sign_bit) raw |= ~((sign_bit << 1u) - 1u);
    }
    return static_cast<int64_t>(raw);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

std::optional<int64_t>
extract_raw(const Signal& sig, std::span<const uint8_t> data) noexcept {
    if (sig.bit_length == 0 || sig.bit_length > 64u) return std::nullopt;
    if (sig.byte_order == ByteOrder::LittleEndian)
        return extract_le(sig, data);
    return extract_be(sig, data);
}

std::optional<double>
extract_signal(const Signal& sig, std::span<const uint8_t> data) noexcept {
    auto raw = extract_raw(sig, data);
    if (!raw) return std::nullopt;
    return static_cast<double>(*raw) * sig.factor + sig.offset;
}

bool pack_signal(const Signal& sig, double physical_value,
                 std::span<uint8_t> data) noexcept {
    if (sig.bit_length == 0 || sig.bit_length > 64u) return false;
    // Range check (only if min != max, i.e. not both zero)
    if (sig.min_val < sig.max_val) {
        if (physical_value < sig.min_val || physical_value > sig.max_val)
            return false;
    }

    // Compute raw value
    double raw_d = (physical_value - sig.offset) / sig.factor;
    auto raw = static_cast<int64_t>(std::round(raw_d));
    uint64_t uraw = static_cast<uint64_t>(raw);

    const uint32_t len = sig.bit_length;
    // Mask to bit_length bits
    uint64_t mask = (len < 64u) ? ((uint64_t{1} << len) - 1u) : ~uint64_t{0};
    uraw &= mask;

    if (sig.byte_order == ByteOrder::LittleEndian) {
        const uint32_t start = sig.start_bit;
        const uint32_t last_bit  = start + len - 1u;
        const uint32_t last_byte = last_bit / 8u;
        if (last_byte >= data.size()) return false;

        for (uint32_t bit = 0; bit < len; ++bit) {
            uint32_t dst_bit  = start + bit;
            uint32_t byte_idx = dst_bit / 8u;
            uint32_t bit_idx  = dst_bit % 8u;
            uint8_t  bit_val  = (uraw >> bit) & 1u;
            data[byte_idx] = static_cast<uint8_t>(
                (data[byte_idx] & ~(1u << bit_idx)) | (bit_val << bit_idx));
        }
    } else {
        // Big-endian
        uint32_t bit_pos = sig.start_bit;
        for (uint32_t i = 0; i < len; ++i) {
            uint32_t byte_idx = bit_pos / 8u;
            if (byte_idx >= data.size()) return false;
            uint32_t bit_idx  = bit_pos % 8u;
            uint8_t  bit_val  = (uraw >> (len - 1u - i)) & 1u;
            data[byte_idx] = static_cast<uint8_t>(
                (data[byte_idx] & ~(1u << bit_idx)) | (bit_val << bit_idx));
            if (bit_idx == 0) bit_pos = (byte_idx + 1u) * 8u + 7u;
            else --bit_pos;
        }
    }
    return true;
}

} // namespace socketspy::dbc
