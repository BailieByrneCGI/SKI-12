#include "rag/json.hpp"
#include "rag/harness.hpp"

#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace rag {
namespace {

void append_utf8(std::string& target, unsigned codepoint) {
    if (codepoint <= 0x7f) target += static_cast<char>(codepoint);
    else if (codepoint <= 0x7ff) {
        target += static_cast<char>(0xc0 | (codepoint >> 6));
        target += static_cast<char>(0x80 | (codepoint & 0x3f));
    } else if (codepoint <= 0xffff) {
        target += static_cast<char>(0xe0 | (codepoint >> 12));
        target += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f));
        target += static_cast<char>(0x80 | (codepoint & 0x3f));
    } else {
        target += static_cast<char>(0xf0 | (codepoint >> 18));
        target += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f));
        target += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f));
        target += static_cast<char>(0x80 | (codepoint & 0x3f));
    }
}

class Parser {
public:
    explicit Parser(const std::string& text) : text_(text) {}
    Json parse() {
        auto value = parse_value(0);
        skip_space();
        if (pos_ != text_.size()) fail("trailing data");
        return value;
    }

private:
    const std::string& text_;
    std::size_t pos_ = 0;

    [[noreturn]] void fail(const char* message) const {
        throw std::runtime_error(std::string("JSON ") + message + " at byte " + std::to_string(pos_));
    }
    void skip_space() {
        while (pos_ < text_.size() && (text_[pos_] == ' ' || text_[pos_] == '\r' ||
               text_[pos_] == '\n' || text_[pos_] == '\t')) ++pos_;
    }
    char take() {
        if (pos_ >= text_.size()) fail("unexpected end");
        return text_[pos_++];
    }
    unsigned hex4() {
        unsigned value = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = take();
            value <<= 4;
            if (c >= '0' && c <= '9') value += static_cast<unsigned>(c - '0');
            else if (c >= 'a' && c <= 'f') value += static_cast<unsigned>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') value += static_cast<unsigned>(c - 'A' + 10);
            else fail("invalid unicode escape");
        }
        return value;
    }
    std::string parse_string() {
        if (take() != '"') fail("expected string");
        std::string result;
        while (pos_ < text_.size()) {
            const char c = take();
            if (c == '"') return result;
            if (static_cast<unsigned char>(c) < 0x20) fail("control character in string");
            if (c != '\\') { result += c; continue; }
            switch (const char escape = take()) {
                case '"': result += '"'; break;
                case '\\': result += '\\'; break;
                case '/': result += '/'; break;
                case 'b': result += '\b'; break;
                case 'f': result += '\f'; break;
                case 'n': result += '\n'; break;
                case 'r': result += '\r'; break;
                case 't': result += '\t'; break;
                case 'u': {
                    unsigned codepoint = hex4();
                    if (codepoint >= 0xd800 && codepoint <= 0xdbff) {
                        if (take() != '\\' || take() != 'u') fail("missing low surrogate");
                        const unsigned low = hex4();
                        if (low < 0xdc00 || low > 0xdfff) fail("invalid low surrogate");
                        codepoint = 0x10000 + ((codepoint - 0xd800) << 10) + (low - 0xdc00);
                    } else if (codepoint >= 0xdc00 && codepoint <= 0xdfff) fail("lone low surrogate");
                    append_utf8(result, codepoint);
                    break;
                }
                default: (void)escape; fail("invalid escape");
            }
        }
        fail("unterminated string");
    }
    Json parse_value(unsigned depth) {
        if (depth > 64) fail("nesting too deep");
        skip_space();
        if (pos_ >= text_.size()) fail("unexpected end");
        if (text_[pos_] == '"') return parse_string();
        if (text_[pos_] == '{') {
            ++pos_;
            Json::Object result;
            skip_space();
            if (pos_ < text_.size() && text_[pos_] == '}') { ++pos_; return result; }
            for (;;) {
                skip_space();
                if (pos_ >= text_.size() || text_[pos_] != '"') fail("expected object key");
                const auto key = parse_string();
                skip_space();
                if (take() != ':') fail("expected colon");
                if (!result.emplace(key, parse_value(depth + 1)).second) fail("duplicate object key");
                skip_space();
                const char separator = take();
                if (separator == '}') break;
                if (separator != ',') fail("expected comma");
            }
            return result;
        }
        if (text_[pos_] == '[') {
            ++pos_;
            Json::Array result;
            skip_space();
            if (pos_ < text_.size() && text_[pos_] == ']') { ++pos_; return result; }
            for (;;) {
                result.push_back(parse_value(depth + 1));
                skip_space();
                const char separator = take();
                if (separator == ']') break;
                if (separator != ',') fail("expected comma");
            }
            return result;
        }
        for (const auto& [literal, value] : {
                 std::pair{"true", Json(true)}, {"false", Json(false)}, {"null", Json(nullptr)}
             }) {
            if (text_.compare(pos_, std::char_traits<char>::length(literal), literal) == 0) {
                pos_ += std::char_traits<char>::length(literal);
                return value;
            }
        }
        const std::size_t start = pos_;
        if (text_[pos_] == '-') ++pos_;
        if (pos_ >= text_.size()) fail("invalid number");
        if (text_[pos_] == '0') ++pos_;
        else {
            if (text_[pos_] < '1' || text_[pos_] > '9') fail("invalid number");
            while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9') ++pos_;
        }
        if (pos_ < text_.size() && text_[pos_] == '.') {
            ++pos_;
            if (pos_ >= text_.size() || text_[pos_] < '0' || text_[pos_] > '9') fail("invalid fraction");
            while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9') ++pos_;
        }
        if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-')) ++pos_;
            if (pos_ >= text_.size() || text_[pos_] < '0' || text_[pos_] > '9') fail("invalid exponent");
            while (pos_ < text_.size() && text_[pos_] >= '0' && text_[pos_] <= '9') ++pos_;
        }
        const double number = std::stod(text_.substr(start, pos_ - start));
        if (!std::isfinite(number)) fail("non-finite number");
        return number;
    }
};

} // namespace

Json Json::parse(const std::string& text) { return Parser(text).parse(); }
bool Json::is_object() const { return std::holds_alternative<Object>(value_); }
bool Json::is_array() const { return std::holds_alternative<Array>(value_); }
bool Json::is_string() const { return std::holds_alternative<std::string>(value_); }
bool Json::is_bool() const { return std::holds_alternative<bool>(value_); }
bool Json::is_number() const { return std::holds_alternative<double>(value_); }
bool Json::is_null() const { return std::holds_alternative<std::nullptr_t>(value_); }
const Json::Object& Json::object() const { return std::get<Object>(value_); }
const Json::Array& Json::array() const { return std::get<Array>(value_); }
const std::string& Json::string() const { return std::get<std::string>(value_); }
bool Json::boolean() const { return std::get<bool>(value_); }
double Json::number() const { return std::get<double>(value_); }
const Json& Json::at(const std::string& key) const {
    static const Json empty;
    if (!is_object()) return empty;
    const auto it = object().find(key);
    return it == object().end() ? empty : it->second;
}
std::string Json::string_or(const std::string& fallback) const {
    return is_string() ? string() : fallback;
}
std::string Json::dump() const {
    if (std::holds_alternative<std::nullptr_t>(value_)) return "null";
    if (is_bool()) return boolean() ? "true" : "false";
    if (is_number()) { std::ostringstream out; out << std::setprecision(15) << number(); return out.str(); }
    if (is_string()) return "\"" + json_escape(string()) + "\"";
    std::string output;
    if (is_array()) {
        output = "[";
        for (const auto& item : array()) { if (output.size() > 1) output += ','; output += item.dump(); }
        return output + "]";
    }
    output = "{";
    for (const auto& [key, value] : object()) {
        if (output.size() > 1) output += ',';
        output += "\"" + json_escape(key) + "\":" + value.dump();
    }
    return output + "}";
}

} // namespace rag
