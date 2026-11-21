#include "codec.h"

#include <cstring>

namespace velk::remote {

namespace {

/// Arrays of arrays deeper than this are refused.
constexpr int kMaxValueDepth = 8;

} // namespace

Writer::Writer(const Header& header)
{
    u8(static_cast<uint8_t>(header.kind));
    u8(static_cast<uint8_t>(header.op));
    u32(header.id);
}

void Writer::raw(const void* data, size_t size)
{
    const auto* p = static_cast<const uint8_t*>(data);
    const size_t at = message_.size();
    message_.resize(at + size);
    if (size) {
        std::memcpy(message_.data() + at, p, size);
    }
}

void Writer::str(string_view s)
{
    u32(static_cast<uint32_t>(s.size()));
    raw(s.data(), s.size());
}

void Writer::value(const RemoteValue& v)
{
    uid(v.type);
    u8(static_cast<uint8_t>(v.form));
    switch (v.form) {
    case RemoteValue::Form::Bytes:
    case RemoteValue::Form::String:
        u32(static_cast<uint32_t>(v.data.size()));
        raw(v.data.data(), v.data.size());
        break;
    case RemoteValue::Form::Object:
        u64(v.object);
        break;
    case RemoteValue::Form::Array:
        uid(v.element_type);
        u32(static_cast<uint32_t>(v.elements.size()));
        for (auto& e : v.elements) {
            value(e);
        }
        break;
    default:
        break;
    }
}

void Writer::object(const RemoteObject& o)
{
    u64(o.handle);
    uid(o.class_uid);
}

Reader::Reader(const Message& message) : message_(message)
{
    header_.kind = static_cast<Kind>(u8());
    header_.op = static_cast<Op>(u8());
    header_.id = u32();
}

bool Reader::raw(void* out, size_t size)
{
    if (!ok_ || size > message_.size() - pos_) {
        ok_ = false;
        return false;
    }
    if (size) {
        std::memcpy(out, message_.data() + pos_, size);
    }
    pos_ += size;
    return true;
}

uint8_t Reader::u8()
{
    uint8_t v = 0;
    raw(&v, sizeof(v));
    return v;
}

uint32_t Reader::u32()
{
    uint32_t v = 0;
    raw(&v, sizeof(v));
    return v;
}

uint64_t Reader::u64()
{
    uint64_t v = 0;
    raw(&v, sizeof(v));
    return v;
}

int32_t Reader::i32()
{
    int32_t v = 0;
    raw(&v, sizeof(v));
    return v;
}

Uid Reader::uid()
{
    Uid v{};
    raw(&v, sizeof(v));
    return v;
}

uint32_t Reader::count()
{
    const uint32_t n = u32();
    // Every list item takes at least one byte, so a count beyond the bytes left is broken.
    if (n > message_.size() - pos_) {
        ok_ = false;
        return 0;
    }
    return n;
}

string Reader::str()
{
    const uint32_t n = count();
    if (!ok_) {
        return {};
    }
    string s(string_view(reinterpret_cast<const char*>(message_.data() + pos_), n));
    pos_ += n;
    return s;
}

bool Reader::value(RemoteValue& out)
{
    return value(out, 0);
}

bool Reader::value(RemoteValue& out, int depth)
{
    if (depth > kMaxValueDepth) {
        ok_ = false;
        return false;
    }
    out.type = uid();
    out.form = static_cast<RemoteValue::Form>(u8());
    switch (out.form) {
    case RemoteValue::Form::None:
        break;
    case RemoteValue::Form::Bytes:
    case RemoteValue::Form::String: {
        const uint32_t n = count();
        out.data.resize(n);
        raw(out.data.data(), n);
        break;
    }
    case RemoteValue::Form::Object:
        out.object = u64();
        break;
    case RemoteValue::Form::Array: {
        out.element_type = uid();
        const uint32_t n = count();
        out.elements.resize(n);
        for (uint32_t i = 0; i < n && ok_; ++i) {
            value(out.elements[i], depth + 1);
        }
        break;
    }
    default:
        ok_ = false;
    }
    return ok_;
}

RemoteObject Reader::object()
{
    RemoteObject o;
    o.handle = u64();
    o.class_uid = uid();
    return o;
}

} // namespace velk::remote
