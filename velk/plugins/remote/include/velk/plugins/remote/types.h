#ifndef VELK_PLUGINS_REMOTE_TYPES_H
#define VELK_PLUGINS_REMOTE_TYPES_H

#include <velk/common.h>
#include <velk/interface/types.h>
#include <velk/string.h>
#include <velk/string_view.h>
#include <velk/vector.h>

#include <cstdint>
#include <cstring>
#include <type_traits>

namespace velk {

/** @brief Identifies an object on the server. Never reused; 0 means none. */
using RemoteHandle = uint64_t;

/**
 * @brief A property value as it travels between server and client.
 *
 * Plain data types (numbers, bool, vectors, colors, matrices, enums) are their
 * memory bytes, so both sides must share the memory layout: build them from
 * the same source.
 */
struct RemoteValue
{
    enum class Form : uint8_t
    {
        None,   ///< No value.
        Bytes,  ///< data holds the value's memory.
        String, ///< data holds UTF-8 text.
        Object, ///< object references an object (ObjectRef properties).
        Array   ///< elements hold the values (array properties).
    };

    Uid type;                     ///< The value's type uid.
    Form form = Form::None;
    vector<uint8_t> data;
    RemoteHandle object = 0;      ///< Form::Object: the referenced object, 0 for an empty reference.
    Uid element_type;             ///< Form::Array: the element type uid.
    vector<RemoteValue> elements; ///< Form::Array: the elements.

    bool operator==(const RemoteValue& o) const
    {
        return type == o.type && form == o.form && data == o.data && object == o.object
               && element_type == o.element_type && elements == o.elements;
    }
    bool operator!=(const RemoteValue& o) const { return !(*this == o); }
};

/** @brief Makes a value of a plain data type. */
template <class T>
RemoteValue make_remote_value(const T& value)
{
    static_assert(std::is_trivially_copyable_v<T>, "use the string or reference overloads");
    RemoteValue v;
    v.type = type_uid<T>();
    v.form = RemoteValue::Form::Bytes;
    v.data.resize(sizeof(T));
    std::memcpy(v.data.data(), &value, sizeof(T));
    return v;
}

/** @brief Makes a string value. */
inline RemoteValue make_remote_value(string_view text)
{
    RemoteValue v;
    v.type = type_uid<string>();
    v.form = RemoteValue::Form::String;
    v.data.resize(text.size());
    if (!text.empty()) {
        std::memcpy(v.data.data(), text.data(), text.size());
    }
    return v;
}

/** @brief Makes an object reference value; @p object 0 clears the reference. */
inline RemoteValue make_remote_reference(RemoteHandle object)
{
    RemoteValue v;
    v.type = ClassId::ObjectRef;
    v.form = RemoteValue::Form::Object;
    v.object = object;
    return v;
}

/** @brief Reads a plain data value. Returns false if @p value holds another type. */
template <class T>
bool read_remote_value(const RemoteValue& value, T& out)
{
    static_assert(std::is_trivially_copyable_v<T>, "use the string overload");
    if (value.form != RemoteValue::Form::Bytes || value.type != type_uid<T>() || value.data.size() != sizeof(T)) {
        return false;
    }
    std::memcpy(&out, value.data.data(), sizeof(T));
    return true;
}

/** @brief Reads a string value. Returns false if @p value is not a string. */
inline bool read_remote_value(const RemoteValue& value, string& out)
{
    if (value.form != RemoteValue::Form::String) {
        return false;
    }
    out = string(string_view(reinterpret_cast<const char*>(value.data.data()), value.data.size()));
    return true;
}

/** @brief An object as the server reports it. */
struct RemoteObject
{
    RemoteHandle handle = 0;
    Uid class_uid;

    bool operator==(const RemoteObject& o) const { return handle == o.handle && class_uid == o.class_uid; }
    bool operator!=(const RemoteObject& o) const { return !(*this == o); }
};

/** @brief An object the server's program published under a name. */
struct RemoteRoot
{
    string name;
    RemoteObject object;

    bool operator==(const RemoteRoot& o) const { return name == o.name && object == o.object; }
    bool operator!=(const RemoteRoot& o) const { return !(*this == o); }
};

/** @brief A property of a described object. */
struct RemoteProperty
{
    string name;
    Uid type;
    Uid interface_uid;           ///< Declaring interface; IDynamicMetadata::UID for dynamic properties.
    uint32_t flags = 0;          ///< ObjectFlags, such as ReadOnly.
    ReturnValue status{};        ///< Fail when the value could not be read or encoded.
    RemoteValue value;

    bool operator==(const RemoteProperty& o) const
    {
        return name == o.name && type == o.type && interface_uid == o.interface_uid && flags == o.flags
               && status == o.status && value == o.value;
    }
    bool operator!=(const RemoteProperty& o) const { return !(*this == o); }
};

/** @brief An event, function or interface of a described object. */
struct RemoteMember
{
    string name;
    Uid uid; ///< Members: the declaring interface. Interfaces: the interface itself.

    bool operator==(const RemoteMember& o) const { return name == o.name && uid == o.uid; }
    bool operator!=(const RemoteMember& o) const { return !(*this == o); }
};

/** @brief Everything the server can tell about one object. */
struct RemoteDescription
{
    RemoteHandle handle = 0;
    bool alive = false; ///< False when the object no longer exists; the rest is empty.
    Uid class_uid;
    string class_name;
    vector<RemoteMember> interfaces;
    vector<RemoteProperty> properties;
    vector<RemoteMember> events;
    vector<RemoteMember> functions;
    vector<RemoteObject> attachments;
    bool is_hierarchy = false;
    RemoteObject hierarchy_root; ///< When is_hierarchy: its root.

    bool operator==(const RemoteDescription& o) const
    {
        return handle == o.handle && alive == o.alive && class_uid == o.class_uid && class_name == o.class_name
               && interfaces == o.interfaces && properties == o.properties && events == o.events
               && functions == o.functions && attachments == o.attachments && is_hierarchy == o.is_hierarchy
               && hierarchy_root == o.hierarchy_root;
    }
    bool operator!=(const RemoteDescription& o) const { return !(*this == o); }
};

/** @brief A node in a hierarchy: children() input and remove() input. */
struct RemoteNode
{
    RemoteHandle hierarchy = 0;
    RemoteHandle node = 0;

    bool operator==(const RemoteNode& o) const { return hierarchy == o.hierarchy && node == o.node; }
    bool operator!=(const RemoteNode& o) const { return !(*this == o); }
};

/** @brief The children of one node. */
struct RemoteChildren
{
    RemoteNode node;
    ReturnValue status{}; ///< Fail when the hierarchy or node does not exist.
    vector<RemoteObject> children;

    bool operator==(const RemoteChildren& o) const
    {
        return node == o.node && status == o.status && children == o.children;
    }
    bool operator!=(const RemoteChildren& o) const { return !(*this == o); }
};

/** @brief A property of an object: get() input. */
struct RemotePropertyName
{
    RemoteHandle object = 0;
    string name;

    bool operator==(const RemotePropertyName& o) const { return object == o.object && name == o.name; }
    bool operator!=(const RemotePropertyName& o) const { return !(*this == o); }
};

/** @brief A property and its value: get() result, set() and add_property() input, change notification. */
struct RemotePropertyValue
{
    RemoteHandle object = 0;
    string name;
    ReturnValue status{}; ///< get(): Fail when the object or property does not exist. Ignored as input.
    RemoteValue value;

    bool operator==(const RemotePropertyValue& o) const
    {
        return object == o.object && name == o.name && status == o.status && value == o.value;
    }
    bool operator!=(const RemotePropertyValue& o) const { return !(*this == o); }
};

/** @brief An attachment on a target object: add_attachment() and remove_attachment() input. */
struct RemoteAttachment
{
    RemoteHandle target = 0;
    RemoteHandle attachment = 0;

    bool operator==(const RemoteAttachment& o) const { return target == o.target && attachment == o.attachment; }
    bool operator!=(const RemoteAttachment& o) const { return !(*this == o); }
};

/** @brief insert() input. A parent of 0 makes the child the hierarchy's root; an index past the end appends. */
struct RemoteInsert
{
    RemoteHandle hierarchy = 0;
    RemoteHandle parent = 0;
    uint32_t index = 0;
    RemoteHandle child = 0;

    bool operator==(const RemoteInsert& o) const
    {
        return hierarchy == o.hierarchy && parent == o.parent && index == o.index && child == o.child;
    }
    bool operator!=(const RemoteInsert& o) const { return !(*this == o); }
};

/// @name Replies and event arguments, each one list wrapped in a struct.
/// @{
template <class T>
struct RemoteList
{
    vector<T> items;

    bool operator==(const RemoteList& o) const { return items == o.items; }
    bool operator!=(const RemoteList& o) const { return !(*this == o); }
};

using RemoteRoots = RemoteList<RemoteRoot>;
using RemoteObjects = RemoteList<RemoteObject>;
using RemoteDescriptions = RemoteList<RemoteDescription>;
using RemoteChildrenList = RemoteList<RemoteChildren>;
using RemotePropertyValues = RemoteList<RemotePropertyValue>;
using RemoteResults = RemoteList<ReturnValue>; ///< One result per input item, in order.
using RemoteHandles = RemoteList<RemoteHandle>;
/// @}

} // namespace velk

#endif // VELK_PLUGINS_REMOTE_TYPES_H
