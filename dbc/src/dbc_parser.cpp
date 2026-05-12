#include "dbc_parser.h"
#include "dbc_lexer.h"
#include <unordered_map>

namespace socketspy::dbc {

using namespace detail;

namespace {

constexpr size_t kMaxFileSize = 64u * 1024u * 1024u;
static const size_t kNoMsg = ~size_t{0};

// Parse one signal line (called after SG_ keyword is consumed).
std::optional<Signal> parse_signal_line(Lexer& lex) noexcept {
    Signal sig;
    auto name_tok = lex.next_token();
    if (name_tok.empty()) return std::nullopt;
    sig.name = std::string(name_tok);

    // Optional mux indicator: M or m<id>
    {
        char pk = lex.peek_char();
        if (pk == 'M' || pk == 'm') {
            size_t saved = lex.pos;
            auto tok = lex.next_token();
            if (tok != "M" && (tok.empty() || tok[0] != 'm'))
                lex.pos = saved;
        }
    }
    if (!lex.try_consume(':')) return std::nullopt;

    auto start_tok = lex.next_token();
    if (!lex.try_consume('|')) return std::nullopt;
    auto len_ot = lex.next_token(); // "8@1+" or "16@0-"

    uint32_t start_u{}, len_u{};
    if (!parse_int(start_tok, start_u)) return std::nullopt;

    auto at = len_ot.find('@');
    if (at == std::string_view::npos || at + 2 >= len_ot.size()) return std::nullopt;
    auto len_tok  = len_ot.substr(0, at);
    auto order_c  = len_ot[at + 1];
    auto type_c   = len_ot[at + 2];

    if (!parse_int(len_tok, len_u) || len_u == 0 || len_u > 64) return std::nullopt;
    if (order_c != '0' && order_c != '1') return std::nullopt;
    if (type_c  != '+' && type_c  != '-') return std::nullopt;

    sig.start_bit  = static_cast<uint8_t>(start_u > 255u ? 255u : start_u);
    sig.bit_length = static_cast<uint8_t>(len_u);
    sig.byte_order = (order_c == '1') ? ByteOrder::LittleEndian : ByteOrder::BigEndian;
    sig.value_type = (type_c  == '+') ? ValueType::Unsigned     : ValueType::Signed;

    if (!lex.try_consume('(')) return std::nullopt;
    auto f_tok = lex.next_token();
    if (!lex.try_consume(',')) return std::nullopt;
    auto o_tok = lex.next_token();
    if (!lex.try_consume(')')) return std::nullopt;
    if (!parse_double(f_tok, sig.factor)) sig.factor = 1.0;
    if (!parse_double(o_tok, sig.offset)) sig.offset = 0.0;

    if (!lex.try_consume('[')) return std::nullopt;
    auto mn_tok = lex.next_token();
    if (!lex.try_consume('|')) return std::nullopt;
    auto mx_tok = lex.next_token();
    if (!lex.try_consume(']')) return std::nullopt;
    if (!parse_double(mn_tok, sig.min_val)) sig.min_val = 0.0;
    if (!parse_double(mx_tok, sig.max_val)) sig.max_val = 0.0;

    sig.unit = lex.read_quoted_string();

    // Receivers — stop at end of line
    lex.skip_inline_ws();
    while (!lex.at_end() && lex.src[lex.pos] != '\n' && lex.src[lex.pos] != '\r') {
        auto recv = lex.next_token();
        if (recv.empty()) break;
        if (recv.back() == ',') recv.remove_suffix(1);
        if (!recv.empty()) sig.receivers.emplace_back(recv);
        lex.skip_inline_ws();
        if (!lex.at_end() && lex.src[lex.pos] == ',') ++lex.pos;
    }
    return sig;
}

size_t find_msg(const std::unordered_map<uint32_t, size_t>& idx,
                uint32_t raw_id) noexcept {
    auto it = idx.find(raw_id);
    if (it != idx.end()) return it->second;
    if (raw_id >= 0x80000000u) {
        it = idx.find((raw_id & 0x1FFFFFFFu) | 0x80000000u);
        if (it != idx.end()) return it->second;
        it = idx.find(raw_id & 0x1FFFFFFFu);
        if (it != idx.end()) return it->second;
    }
    return kNoMsg;
}

} // anonymous namespace

std::expected<DbcDatabase, ParseError>
parse_dbc(std::string_view input) noexcept {
    if (input.size() > kMaxFileSize)
        return std::unexpected(ParseError::FileTooLarge);

    DbcDatabase db;
    db.messages.reserve(64);
    Lexer lex{input, 0};
    std::unordered_map<uint32_t, size_t> msg_idx;
    msg_idx.reserve(64);
    size_t cur = kNoMsg;

    while (!lex.at_end()) {
        lex.skip_ws();
        if (lex.at_end()) break;
        auto kw = lex.next_token();
        if (kw.empty()) { lex.consume_line(); continue; }

        if (kw == "VERSION") {
            db.version = lex.read_quoted_string();
        } else if (kw == "NS_") {
            lex.try_consume(':');
            lex.consume_line();
            while (!lex.at_end() && (lex.src[lex.pos]==' '||lex.src[lex.pos]=='\t'))
                lex.consume_line();
        } else if (kw == "BU_") {
            lex.try_consume(':');
            while (!lex.at_end()) {
                while (!lex.at_end() && (lex.src[lex.pos]==' '||lex.src[lex.pos]=='\t'))
                    ++lex.pos;
                if (lex.at_end()||lex.src[lex.pos]=='\n'||lex.src[lex.pos]=='\r') break;
                auto node = lex.next_token();
                if (node.empty()) break;
                db.nodes.emplace_back(node);
            }
        } else if (kw == "BO_") {
            cur = kNoMsg;
            Message msg;
            uint32_t raw_id{};
            if (!parse_int(lex.next_token(), raw_id)) { lex.consume_line(); continue; }
            msg.extended = (raw_id >= 0x80000000u);
            msg.id = msg.extended ? (raw_id & 0x1FFFFFFFu) : raw_id;
            uint32_t canon = msg.id | (msg.extended ? 0x80000000u : 0u);
            msg.name = std::string(lex.next_token());
            lex.try_consume(':');
            uint32_t dlc{};
            if (!parse_int(lex.next_token(), dlc)) dlc = 0;
            msg.dlc = static_cast<uint8_t>(dlc > 64u ? 64u : dlc);
            msg.transmitter = std::string(lex.next_token());
            if (msg_idx.count(canon)) continue;
            cur = db.messages.size();
            db.messages.push_back(std::move(msg));
            msg_idx[canon] = cur;
            continue; // preserve cur for SG_ lines
        } else if (kw == "SG_") {
            if (cur != kNoMsg) {
                auto sig = parse_signal_line(lex);
                if (sig) db.messages[cur].signals.push_back(std::move(*sig));
            } else {
                lex.consume_line();
            }
            continue;
        } else if (kw == "CM_") {
            auto sub = lex.next_token();
            if (sub == "SG_") {
                uint32_t rid{};
                if (!parse_int(lex.next_token(), rid)) { lex.consume_line(); continue; }
                auto sname = lex.next_token();
                auto cmt   = lex.read_quoted_string();
                lex.try_consume(';');
                size_t mi = find_msg(msg_idx, rid);
                if (mi != kNoMsg)
                    for (auto& s : db.messages[mi].signals)
                        if (s.name == sname) { s.attributes.emplace_back("comment", cmt); break; }
            } else if (sub == "BO_") {
                uint32_t rid{};
                if (!parse_int(lex.next_token(), rid)) { lex.consume_line(); continue; }
                auto cmt = lex.read_quoted_string();
                lex.try_consume(';');
                size_t mi = find_msg(msg_idx, rid);
                if (mi != kNoMsg) db.messages[mi].comment = cmt;
            } else {
                lex.consume_line();
            }
        } else if (kw == "VAL_") {
            uint32_t rid{};
            if (!parse_int(lex.next_token(), rid)) { lex.consume_line(); continue; }
            std::string sname(lex.next_token());
            size_t mi = find_msg(msg_idx, rid);
            Signal* sp = nullptr;
            if (mi != kNoMsg)
                for (auto& s : db.messages[mi].signals)
                    if (s.name == sname) { sp = &s; break; }
            while (!lex.at_end()) {
                lex.skip_ws();
                if (lex.at_end()) break;
                char c = lex.src[lex.pos];
                if (c == ';') { ++lex.pos; break; }
                if (c == '"') { lex.read_quoted_string(); continue; }
                int64_t rv{};
                auto vt = lex.next_token();
                if (vt.empty() || vt == ";") break;
                if (!parse_int(vt, rv)) { lex.consume_line(); break; }
                lex.skip_ws();
                if (lex.at_end() || lex.src[lex.pos] != '"') break;
                auto lbl = lex.read_quoted_string();
                if (sp) sp->value_descriptions[rv] = lbl;
            }
        } else if (kw == "BA_DEF_" || kw == "BA_DEF_DEF_") {
            db.raw_attribute_defs.emplace_back(std::string(kw) + " " + std::string(lex.peek_line()));
            lex.consume_line();
        } else if (kw == "EV_") {
            db.raw_env_vars.emplace_back("EV_ " + std::string(lex.peek_line()));
            lex.consume_line();
        } else {
            lex.consume_line();
        }
        cur = kNoMsg; // reset on any non-SG_/BO_ keyword
    }
    return db;
}

} // namespace socketspy::dbc
