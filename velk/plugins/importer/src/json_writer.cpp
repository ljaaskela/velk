#include "json_writer.h"

#include <velk/json.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace velk {

namespace {

/// The text in @p buffer up to its terminator, not the whole array.
string_view text_of(const char* buffer)
{
    return string_view(buffer, std::strlen(buffer));
}

} // namespace

void JsonWriter::before_value()
{
    if (after_key_) {
        after_key_ = false;
        return;
    }
    if (!first_.empty()) {
        if (!first_.back()) {
            out_ += ",";
        }
        first_.back() = 0;
    }
}

void JsonWriter::quoted(string_view value)
{
    json_quote(out_, value);
}

void JsonWriter::null()
{
    before_value();
    out_ += "null";
}

void JsonWriter::boolean(bool value)
{
    before_value();
    out_ += value ? string_view("true") : string_view("false");
}

void JsonWriter::integer(int64_t value)
{
    before_value();
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(value));
    out_ += text_of(buf);
}

void JsonWriter::number(float value)
{
    before_value();
    char buf[32];
    for (int digits = 6; digits <= 9; ++digits) {
        std::snprintf(buf, sizeof(buf), "%.*g", digits, static_cast<double>(value));
        if (std::strtof(buf, nullptr) == value) {
            break;
        }
    }
    out_ += text_of(buf);
}

void JsonWriter::number(double value)
{
    before_value();
    char buf[40];
    for (int digits = 15; digits <= 17; ++digits) {
        std::snprintf(buf, sizeof(buf), "%.*g", digits, value);
        if (std::strtod(buf, nullptr) == value) {
            break;
        }
    }
    out_ += text_of(buf);
}

void JsonWriter::text(string_view value)
{
    before_value();
    quoted(value);
}

void JsonWriter::begin_object()
{
    before_value();
    out_ += "{";
    first_.push_back(1);
}

void JsonWriter::key(string_view name)
{
    before_value();
    quoted(name);
    out_ += ":";
    after_key_ = true;
}

void JsonWriter::end_object()
{
    if (!first_.empty()) {
        first_.pop_back();
    }
    out_ += "}";
}

void JsonWriter::begin_array()
{
    before_value();
    out_ += "[";
    first_.push_back(1);
}

void JsonWriter::end_array()
{
    if (!first_.empty()) {
        first_.pop_back();
    }
    out_ += "]";
}

} // namespace velk
