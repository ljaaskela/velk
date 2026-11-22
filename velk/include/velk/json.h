#ifndef VELK_JSON_H
#define VELK_JSON_H

#include <velk/string.h>
#include <velk/string_view.h>
#include <velk/vector.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>

namespace velk {

/** @brief The kind of a JsonValue. */
enum class JsonType
{
    Null,
    Bool,
    Number,
    String,
    Array,
    Object
};

/**
 * @brief A JSON value: null, bool, number, string, array or object.
 *
 * Object members keep their order. A parsed number keeps its text, so writing it back gives
 * the same characters. Header-only; not for use in interfaces.
 */
class JsonValue
{
public:
    using Member = std::pair<::velk::string, JsonValue>;

    JsonValue() = default;

    static JsonValue null() { return {}; }
    static JsonValue boolean(bool v)
    {
        JsonValue j;
        j.type_ = JsonType::Bool;
        j.bool_ = v;
        return j;
    }
    static JsonValue number(double v)
    {
        JsonValue j;
        j.type_ = JsonType::Number;
        j.number_ = v;
        return j;
    }
    /** @brief A number with the text it is written as. */
    static JsonValue number(double v, ::velk::string text)
    {
        JsonValue j = number(v);
        j.string_ = std::move(text);
        return j;
    }
    static JsonValue string(::velk::string v)
    {
        JsonValue j;
        j.type_ = JsonType::String;
        j.string_ = std::move(v);
        return j;
    }
    static JsonValue array(::velk::vector<JsonValue> v = {})
    {
        JsonValue j;
        j.type_ = JsonType::Array;
        j.array_ = std::move(v);
        return j;
    }
    static JsonValue object(::velk::vector<Member> v = {})
    {
        JsonValue j;
        j.type_ = JsonType::Object;
        j.object_ = std::move(v);
        return j;
    }

    JsonType type() const { return type_; }
    bool is_null() const { return type_ == JsonType::Null; }
    bool is_object() const { return type_ == JsonType::Object; }
    bool is_array() const { return type_ == JsonType::Array; }

    const ::velk::string& as_string() const { return string_; }
    double as_number() const { return number_; }
    bool as_bool() const { return bool_; }
    const ::velk::vector<JsonValue>& as_array() const { return array_; }
    ::velk::vector<JsonValue>& as_array() { return array_; }
    const ::velk::vector<Member>& as_object() const { return object_; }
    ::velk::vector<Member>& as_object() { return object_; }

    /** @brief A number's text as parsed; empty for numbers made from a double. */
    string_view number_text() const { return type_ == JsonType::Number ? string_view(string_) : string_view(); }

    /** @brief Elements of an array or members of an object; 0 otherwise. */
    size_t size() const
    {
        return type_ == JsonType::Array ? array_.size() : type_ == JsonType::Object ? object_.size() : 0;
    }

    /** @brief The member named @p key, or null when absent or not an object. */
    const JsonValue* find(string_view key) const { return const_cast<JsonValue*>(this)->find(key); }
    JsonValue* find(string_view key)
    {
        if (type_ != JsonType::Object) {
            return nullptr;
        }
        for (auto& m : object_) {
            if (string_view(m.first) == key) {
                return &m.second;
            }
        }
        return nullptr;
    }

    /** @brief The member named @p key, or a null value. */
    const JsonValue& operator[](string_view key) const
    {
        auto* v = find(key);
        return v ? *v : null_value();
    }

    /** @brief The element at @p index, or a null value. */
    const JsonValue& operator[](size_t index) const
    {
        return type_ == JsonType::Array && index < array_.size() ? array_[index] : null_value();
    }

    /**
     * @brief Adds a member at the end, or at @p at.
     * @return false when this is not an object or @p key is already there.
     */
    bool insert(string_view key, JsonValue value, size_t at = static_cast<size_t>(-1))
    {
        if (type_ != JsonType::Object || find(key)) {
            return false;
        }
        Member m{::velk::string(key), std::move(value)};
        if (at >= object_.size()) {
            object_.push_back(std::move(m));
        } else {
            object_.insert(object_.begin() + at, std::move(m));
        }
        return true;
    }

    /** @brief Replaces the member named @p key in place. Returns false when it is absent. */
    bool replace(string_view key, JsonValue value)
    {
        auto* v = find(key);
        if (!v) {
            return false;
        }
        *v = std::move(value);
        return true;
    }

    /** @brief Replaces the member named @p key, or adds it at the end. Returns false when not an object. */
    bool set(string_view key, JsonValue value)
    {
        return replace(key, value) || insert(key, std::move(value));
    }

    /** @brief Removes the member named @p key. Returns false when it is absent. */
    bool remove(string_view key)
    {
        if (type_ != JsonType::Object) {
            return false;
        }
        for (auto& m : object_) {
            if (string_view(m.first) == key) {
                object_.erase(&m);
                return true;
            }
        }
        return false;
    }

    /** @brief True when both hold the same value; numbers compare by value, members by order. */
    bool operator==(const JsonValue& o) const
    {
        if (type_ != o.type_) {
            return false;
        }
        switch (type_) {
        case JsonType::Null: return true;
        case JsonType::Bool: return bool_ == o.bool_;
        case JsonType::Number: return number_ == o.number_;
        case JsonType::String: return string_view(string_) == string_view(o.string_);
        case JsonType::Array:
            if (array_.size() != o.array_.size()) {
                return false;
            }
            for (size_t i = 0; i < array_.size(); ++i) {
                if (!(array_[i] == o.array_[i])) {
                    return false;
                }
            }
            return true;
        case JsonType::Object:
            if (object_.size() != o.object_.size()) {
                return false;
            }
            for (size_t i = 0; i < object_.size(); ++i) {
                if (string_view(object_[i].first) != string_view(o.object_[i].first)
                    || !(object_[i].second == o.object_[i].second)) {
                    return false;
                }
            }
            return true;
        }
        return false;
    }
    bool operator!=(const JsonValue& o) const { return !(*this == o); }

private:
    static const JsonValue& null_value()
    {
        static const JsonValue v;
        return v;
    }

    JsonType type_{JsonType::Null};
    double number_{0.0};
    bool bool_{false};
    ::velk::string string_; ///< A string's value, or a parsed number's text.
    ::velk::vector<JsonValue> array_;
    ::velk::vector<Member> object_;
};

namespace detail {

class JsonParser
{
public:
    JsonParser(const char* data, size_t length) : data_(data), end_(data + length), pos_(data) {}

    bool parse(JsonValue& out, ::velk::string& err)
    {
        skip_whitespace();
        out = parse_value();
        if (!failed_) {
            skip_whitespace();
            if (pos_ != end_) {
                set_error("unexpected trailing content");
            }
        }
        if (failed_) {
            err = error_;
            return false;
        }
        return true;
    }

private:
    const char* data_;
    const char* end_;
    const char* pos_;
    bool failed_ = false;
    ::velk::string error_;

    void set_error(const char* msg)
    {
        if (failed_) {
            return;
        }
        failed_ = true;
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%zu", static_cast<size_t>(pos_ - data_));
        error_ = ::velk::string("JSON parse error at offset ") + buf + ": " + msg;
    }

    char peek()
    {
        if (pos_ >= end_) {
            set_error("unexpected end of input");
            return '\0';
        }
        return *pos_;
    }

    char advance()
    {
        if (pos_ >= end_) {
            set_error("unexpected end of input");
            return '\0';
        }
        return *pos_++;
    }

    bool expect(char c)
    {
        if (failed_) {
            return false;
        }
        char got = advance();
        if (failed_) {
            return false;
        }
        if (got != c) {
            pos_--;
            char msg[] = "expected 'X'";
            msg[10] = c;
            set_error(msg);
            return false;
        }
        return true;
    }

    void skip_whitespace()
    {
        while (pos_ < end_ && (*pos_ == ' ' || *pos_ == '\t' || *pos_ == '\n' || *pos_ == '\r')) {
            pos_++;
        }
    }

    bool literal(const char* word)
    {
        const size_t n = std::strlen(word);
        if (static_cast<size_t>(end_ - pos_) >= n && std::memcmp(pos_, word, n) == 0) {
            pos_ += n;
            return true;
        }
        return false;
    }

    JsonValue parse_value()
    {
        if (failed_) {
            return {};
        }
        skip_whitespace();
        char c = peek();
        if (failed_) {
            return {};
        }
        switch (c) {
        case '"': {
            auto s = parse_string();
            return failed_ ? JsonValue() : JsonValue::string(std::move(s));
        }
        case '{': return parse_object();
        case '[': return parse_array();
        default: break;
        }
        if (literal("null")) {
            return JsonValue::null();
        }
        if (literal("true")) {
            return JsonValue::boolean(true);
        }
        if (literal("false")) {
            return JsonValue::boolean(false);
        }
        if (c == '-' || (c >= '0' && c <= '9')) {
            return parse_number();
        }
        set_error(c == 'n' || c == 't' || c == 'f' ? "invalid literal" : "unexpected character");
        return {};
    }

    bool digits()
    {
        if (pos_ >= end_ || *pos_ < '0' || *pos_ > '9') {
            return false;
        }
        while (pos_ < end_ && *pos_ >= '0' && *pos_ <= '9') {
            pos_++;
        }
        return true;
    }

    JsonValue parse_number()
    {
        const char* start = pos_;
        if (*pos_ == '-') {
            pos_++;
        }
        if (pos_ < end_ && *pos_ == '0') {
            pos_++;
        } else if (!digits()) {
            set_error("invalid number");
            return {};
        }
        if (pos_ < end_ && *pos_ == '.') {
            pos_++;
            if (!digits()) {
                set_error("invalid number: expected digit after decimal point");
                return {};
            }
        }
        if (pos_ < end_ && (*pos_ == 'e' || *pos_ == 'E')) {
            pos_++;
            if (pos_ < end_ && (*pos_ == '+' || *pos_ == '-')) {
                pos_++;
            }
            if (!digits()) {
                set_error("invalid number: expected digit in exponent");
                return {};
            }
        }
        ::velk::string text(start, static_cast<size_t>(pos_ - start));
        double value = std::strtod(text.c_str(), nullptr);
        return JsonValue::number(value, std::move(text));
    }

    uint32_t parse_hex4()
    {
        if (end_ - pos_ < 4) {
            set_error("incomplete unicode escape");
            return 0;
        }
        uint32_t value = 0;
        for (int i = 0; i < 4; i++) {
            char c = *pos_++;
            value <<= 4;
            if (c >= '0' && c <= '9') {
                value |= static_cast<uint32_t>(c - '0');
            } else if (c >= 'a' && c <= 'f') {
                value |= static_cast<uint32_t>(c - 'a' + 10);
            } else if (c >= 'A' && c <= 'F') {
                value |= static_cast<uint32_t>(c - 'A' + 10);
            } else {
                set_error("invalid hex digit in unicode escape");
                return 0;
            }
        }
        return value;
    }

    static void encode_utf8(::velk::string& out, uint32_t cp)
    {
        if (cp < 0x80) {
            out += static_cast<char>(cp);
        } else if (cp < 0x800) {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    ::velk::string parse_string()
    {
        if (!expect('"')) {
            return {};
        }
        ::velk::string result;
        while (true) {
            if (pos_ >= end_) {
                set_error("unterminated string");
                return {};
            }
            char c = *pos_++;
            if (c == '"') {
                return result;
            }
            if (c != '\\') {
                result += c;
                continue;
            }
            if (pos_ >= end_) {
                set_error("unterminated escape");
                return {};
            }
            char esc = *pos_++;
            switch (esc) {
            case '"': result += '"'; break;
            case '\\': result += '\\'; break;
            case '/': result += '/'; break;
            case 'b': result += '\b'; break;
            case 'f': result += '\f'; break;
            case 'n': result += '\n'; break;
            case 'r': result += '\r'; break;
            case 't': result += '\t'; break;
            case 'u': {
                uint32_t cp = parse_hex4();
                if (failed_) {
                    return {};
                }
                if (cp >= 0xD800 && cp <= 0xDBFF && end_ - pos_ >= 2 && pos_[0] == '\\' && pos_[1] == 'u') {
                    pos_ += 2;
                    uint32_t lo = parse_hex4();
                    if (failed_) {
                        return {};
                    }
                    if (lo >= 0xDC00 && lo <= 0xDFFF) {
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    }
                }
                encode_utf8(result, cp);
                break;
            }
            default: set_error("invalid escape character"); return {};
            }
        }
    }

    JsonValue parse_object()
    {
        if (!expect('{')) {
            return {};
        }
        ::velk::vector<JsonValue::Member> members;
        skip_whitespace();
        if (pos_ < end_ && *pos_ == '}') {
            pos_++;
            return JsonValue::object(std::move(members));
        }
        while (true) {
            skip_whitespace();
            auto key = parse_string();
            skip_whitespace();
            if (failed_ || !expect(':')) {
                return {};
            }
            JsonValue value = parse_value();
            if (failed_) {
                return {};
            }
            members.push_back({std::move(key), std::move(value)});
            skip_whitespace();
            char c = advance();
            if (failed_) {
                return {};
            }
            if (c == '}') {
                return JsonValue::object(std::move(members));
            }
            if (c != ',') {
                pos_--;
                set_error("expected ',' or '}'");
                return {};
            }
        }
    }

    JsonValue parse_array()
    {
        if (!expect('[')) {
            return {};
        }
        ::velk::vector<JsonValue> elements;
        skip_whitespace();
        if (pos_ < end_ && *pos_ == ']') {
            pos_++;
            return JsonValue::array(std::move(elements));
        }
        while (true) {
            elements.push_back(parse_value());
            if (failed_) {
                return {};
            }
            skip_whitespace();
            char c = advance();
            if (failed_) {
                return {};
            }
            if (c == ']') {
                return JsonValue::array(std::move(elements));
            }
            if (c != ',') {
                pos_--;
                set_error("expected ',' or ']'");
                return {};
            }
        }
    }
};

} // namespace detail

/**
 * @brief Parses JSON text.
 * @param out   Receives the value on success.
 * @param error Receives the reason on failure, with the byte offset.
 * @return true on success.
 */
inline bool json_parse(string_view text, JsonValue& out, ::velk::string& error)
{
    if (text.empty()) {
        error = "JSON parse error: empty input";
        return false;
    }
    return detail::JsonParser(text.data(), text.size()).parse(out, error);
}

/** @copydoc json_parse(string_view, JsonValue&, ::velk::string&) */
inline bool json_parse(const char* input, size_t length, JsonValue& out, ::velk::string& error)
{
    return json_parse(string_view(input ? input : "", input ? length : 0), out, error);
}

/** @brief Appends @p value to @p out as a quoted JSON string. */
inline void json_quote(::velk::string& out, string_view value)
{
    out += '"';
    for (char c : value) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                char escaped[8];
                std::snprintf(escaped, sizeof(escaped), "\\u%04x", static_cast<unsigned>(c));
                out += string_view(escaped, 6);
            } else {
                out += c;
            }
        }
    }
    out += '"';
}

/** @brief Appends the shortest text that reads back as @p value. */
inline void json_number(::velk::string& out, double value)
{
    char buf[40];
    for (int digits = 15; digits <= 17; ++digits) {
        std::snprintf(buf, sizeof(buf), "%.*g", digits, value);
        if (std::strtod(buf, nullptr) == value) {
            break;
        }
    }
    out += string_view(buf, std::strlen(buf));
}

/** @brief Layout of json_write's output. */
struct JsonWriteOptions
{
    int indent{2};          ///< Spaces per level.
    size_t line_width{100}; ///< An array or object goes on one line when it fits in this many columns.
};

namespace detail {

inline void json_inline(::velk::string& out, const JsonValue& v)
{
    switch (v.type()) {
    case JsonType::Null: out += "null"; return;
    case JsonType::Bool: out += v.as_bool() ? string_view("true") : string_view("false"); return;
    case JsonType::Number:
        if (!v.number_text().empty()) {
            out += v.number_text();
        } else {
            json_number(out, v.as_number());
        }
        return;
    case JsonType::String: json_quote(out, v.as_string()); return;
    case JsonType::Array: {
        out += '[';
        bool first = true;
        for (auto& e : v.as_array()) {
            out += first ? string_view("") : string_view(", ");
            first = false;
            json_inline(out, e);
        }
        out += ']';
        return;
    }
    case JsonType::Object: {
        if (v.as_object().empty()) {
            out += "{}";
            return;
        }
        out += "{ ";
        bool first = true;
        for (auto& m : v.as_object()) {
            out += first ? string_view("") : string_view(", ");
            first = false;
            json_quote(out, m.first);
            out += ": ";
            json_inline(out, m.second);
        }
        out += " }";
        return;
    }
    }
}

inline void json_block(::velk::string& out, const JsonValue& v, size_t column, int depth, const JsonWriteOptions& o)
{
    const bool container = v.is_array() || v.is_object();
    if (!container || v.size() == 0) {
        json_inline(out, v);
        return;
    }
    if (depth > 0) {
        ::velk::string line;
        json_inline(line, v);
        if (column + line.size() <= o.line_width) {
            out += line;
            return;
        }
    }
    const size_t pad = static_cast<size_t>(o.indent) * static_cast<size_t>(depth + 1);
    out += v.is_array() ? '[' : '{';
    bool first = true;
    if (v.is_array()) {
        for (auto& e : v.as_array()) {
            out += first ? string_view("\n") : string_view(",\n");
            first = false;
            out.append(pad, ' ');
            json_block(out, e, pad, depth + 1, o);
        }
    } else {
        for (auto& m : v.as_object()) {
            out += first ? string_view("\n") : string_view(",\n");
            first = false;
            out.append(pad, ' ');
            const size_t start = out.size();
            json_quote(out, m.first);
            out += ": ";
            json_block(out, m.second, pad + (out.size() - start), depth + 1, o);
        }
    }
    out += '\n';
    out.append(pad - static_cast<size_t>(o.indent), ' ');
    out += v.is_array() ? ']' : '}';
}

} // namespace detail

/**
 * @brief Writes @p value as indented JSON text ending in a newline.
 *
 * Members keep their order and parsed numbers their text. An array or object goes on one line
 * when it fits in JsonWriteOptions::line_width, so editing a value changes few lines.
 */
inline ::velk::string json_write(const JsonValue& value, const JsonWriteOptions& options = {})
{
    ::velk::string out;
    detail::json_block(out, value, 0, 0, options);
    out += '\n';
    return out;
}

/** @brief Writes @p value as JSON text on one line. */
inline ::velk::string json_write_inline(const JsonValue& value)
{
    ::velk::string out;
    detail::json_inline(out, value);
    return out;
}

} // namespace velk

#endif // VELK_JSON_H
