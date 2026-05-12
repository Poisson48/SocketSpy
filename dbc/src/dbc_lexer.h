#pragma once
// Internal DBC lexer — not part of the public API.
#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace socketspy::dbc::detail {

struct Lexer {
    std::string_view src;
    size_t pos{0};

    bool at_end() const noexcept { return pos >= src.size(); }

    void skip_ws() noexcept {
        while (!at_end()) {
            char c = src[pos];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                ++pos;
            } else if (c == '/' && pos + 1 < src.size() && src[pos + 1] == '/') {
                while (!at_end() && src[pos] != '\n') ++pos;
            } else {
                break;
            }
        }
    }

    // Skip spaces/tabs only — stops at newlines.
    void skip_inline_ws() noexcept {
        while (!at_end()) {
            char c = src[pos];
            if (c == ' ' || c == '\t' || c == '\r') ++pos;
            else break;
        }
    }

    std::string_view peek_line() noexcept {
        size_t s = pos, e = pos;
        while (e < src.size() && src[e] != '\n') ++e;
        return src.substr(s, e - s);
    }

    void consume_line() noexcept {
        while (!at_end() && src[pos] != '\n') ++pos;
        if (!at_end()) ++pos;
    }

    std::string_view next_token() noexcept {
        skip_ws();
        if (at_end()) return {};
        size_t s = pos;
        while (!at_end()) {
            char c = src[pos];
            if (c==' '||c=='\t'||c=='\r'||c=='\n'||c==':'||c==','||c==';'||
                c=='|'||c=='('||c==')'||c=='['||c==']') break;
            ++pos;
        }
        return src.substr(s, pos - s);
    }

    std::string read_quoted_string() noexcept {
        skip_ws();
        if (at_end() || src[pos] != '"') return {};
        ++pos;
        std::string r;
        r.reserve(32);
        while (!at_end()) {
            char c = src[pos++];
            if (c == '"') return r;
            if (c == '\\' && !at_end()) {
                char e = src[pos++];
                if (e=='"') r+='"'; else if (e=='\\') r+='\\';
                else if (e=='n') r+='\n'; else if (e=='t') r+='\t';
                else { r+='\\'; r+=e; }
            } else { r += c; }
        }
        return r;
    }

    bool try_consume(char ch) noexcept {
        skip_ws();
        if (!at_end() && src[pos] == ch) { ++pos; return true; }
        return false;
    }

    char peek_char() noexcept {
        skip_ws();
        return at_end() ? '\0' : src[pos];
    }
};

template<typename T>
bool parse_int(std::string_view tok, T& out) noexcept {
    if (tok.empty()) return false;
    T val{};
    bool neg = (tok[0] == '-');
    if (neg) tok.remove_prefix(1);
    if (tok.empty()) return false;
    int base = (tok.size() > 2 && tok[0]=='0' && (tok[1]=='x'||tok[1]=='X')) ? 16 : 10;
    auto [ptr, ec] = std::from_chars(tok.data(), tok.data() + tok.size(), val, base);
    if (ec != std::errc{} || ptr != tok.data() + tok.size()) return false;
    if (neg) val = static_cast<T>(-static_cast<int64_t>(val));
    out = val;
    return true;
}

inline bool parse_double(std::string_view tok, double& out) noexcept {
    if (tok.empty()) return false;
    auto [ptr, ec] = std::from_chars(tok.data(), tok.data() + tok.size(), out);
    return ec == std::errc{};
}

} // namespace socketspy::dbc::detail
