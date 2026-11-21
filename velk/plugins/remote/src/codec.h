#ifndef VELK_REMOTE_CODEC_H
#define VELK_REMOTE_CODEC_H

#include "connection.h"

#include <velk/plugins/remote/types.h>
#include <velk/string.h>

namespace velk::remote {

/// Raised when the message layout changes; a client and server must match.
inline constexpr uint32_t kProtocolVersion = 1;

enum class Kind : uint8_t
{
    Request = 1,
    Reply = 2,
    Notification = 3
};

enum class Op : uint8_t
{
    Hello = 1,
    Roots,
    Instances,
    Describe,
    Children,
    Get,
    Set,
    Subscribe,
    Unsubscribe,
    Create,
    Release,
    AddAttachment,
    RemoveAttachment,
    AddProperty,
    Insert,
    Remove,
    Call,

    Changed = 64, ///< Notification: RemotePropertyValues.
    Released      ///< Notification: handles of objects that no longer exist.
};

/// Every message starts with kind, op and a request id (0 for notifications).
struct Header
{
    Kind kind{};
    Op op{};
    uint32_t id = 0;
};

/** @brief Appends values in the wire layout: little-endian, lengths as u32. */
class Writer
{
public:
    explicit Writer(const Header& header);

    void u8(uint8_t v) { message_.push_back(v); }
    void u32(uint32_t v) { raw(&v, sizeof(v)); }
    void u64(uint64_t v) { raw(&v, sizeof(v)); }
    void i32(int32_t v) { raw(&v, sizeof(v)); }
    void uid(Uid v) { raw(&v, sizeof(v)); }
    void str(string_view s);
    void value(const RemoteValue& v);
    void object(const RemoteObject& o);
    void raw(const void* data, size_t size);

    Message& message() { return message_; }

private:
    Message message_;
};

/** @brief Reads what Writer wrote. Any read past the end, or malformed data, sets !ok(). */
class Reader
{
public:
    explicit Reader(const Message& message);

    const Header& header() const { return header_; }

    uint8_t u8();
    uint32_t u32();
    uint64_t u64();
    int32_t i32();
    Uid uid();
    string str();
    bool value(RemoteValue& out);
    RemoteObject object();
    /** @brief A list length, checked against the bytes left so a broken length cannot allocate wildly. */
    uint32_t count();

    bool ok() const { return ok_; }

private:
    bool raw(void* out, size_t size);
    bool value(RemoteValue& out, int depth);

    const Message& message_;
    size_t pos_ = 0;
    bool ok_ = true;
    Header header_;
};

} // namespace velk::remote

#endif // VELK_REMOTE_CODEC_H
