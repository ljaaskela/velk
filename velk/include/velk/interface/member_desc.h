#ifndef VELK_MEMBER_DESC_H
#define VELK_MEMBER_DESC_H

#include <velk/api/any.h>
#include <velk/api/traits.h>
#include <velk/array_view.h>
#include <velk/common.h>
#include <velk/interface/intf_interface.h>

#include <cstdint>

namespace velk {

/** @brief Discriminator for the kind of member described by a MemberDesc. */
enum class MemberKind : uint8_t
{
    Property,
    Event,
    Function
};

/** @brief Discriminator for the kind of notification to broadcast. */
enum class Notification : uint8_t
{
    Changed,
    Invoked,
    Reset
};

/** @brief Function pointer type for trampoline callbacks that route to virtual methods. */
using FnTrampoline = IAny::Ptr (*)(void* self, FnArgs args);

/** @brief Kind-specific data for Property members. */
struct PropertyKind
{
    Uid typeUid; ///< type_uid<T>() for the property's value type.
    /** @brief Returns a pointer to a static IAny holding the property's default value. */
    const IAny* (*getDefault)() = nullptr;
    /** @brief Creates an AnyRef pointing into the State struct at @p stateBase. */
    IAny::Ptr (*createRef)(void* stateBase) = nullptr;
    int32_t flags{ObjectFlags::None}; ///< ObjectFlags to apply to the created PropertyImpl.
};

/** @brief Describes a single argument of a typed function. */
struct FnArgDesc
{
    string_view name; ///< Parameter name (e.g. "x").
    Uid typeUid;      ///< type_uid<T>() for the parameter type.
};

/** @brief Kind-specific data for Function/Event members. */
struct FunctionKind
{
    FnTrampoline trampoline = nullptr; ///< Static trampoline that routes invoke() to the virtual method.
    array_view<FnArgDesc> args;        ///< Typed argument descriptors; empty for zero-arg and FN_RAW.
};

/** @brief Describes a single member (property, event, or function) declared by an object class. */
struct MemberDesc
{
    string_view name;                   ///< Member name used for runtime lookup.
    MemberKind kind;                    ///< Discriminator (Property, Event, or Function).
    const InterfaceInfo* interfaceInfo; ///< Interface that declared this member.
    const void* ext = nullptr;          ///< Points to PropertyKind or FunctionKind based on @c kind.

    /// Typed ext member getter for MemberDesc.kind = MemberKind::Property
    constexpr const PropertyKind* propertyKind() const
    {
        return kind == MemberKind::Property ? static_cast<const PropertyKind*>(ext) : nullptr;
    }
    /// Typed ext member getter for MemberDesc.kind = MemberKind::Function or MemberDesc.kind =
    /// MemberKind::Event
    constexpr const FunctionKind* functionKind() const
    {
        return (kind == MemberKind::Function || kind == MemberKind::Event)
                   ? static_cast<const FunctionKind*>(ext)
                   : nullptr;
    }
};

/**
 * @brief Creates a Property MemberDesc.
 * @param name Member name for runtime lookup.
 * @param info Interface that declares this member (may be nullptr).
 * @param pk PropertyKind with typeUid, getDefault, and createRef (may be nullptr).
 */
constexpr MemberDesc PropertyDesc(string_view name, const InterfaceInfo* info = nullptr,
                                  const PropertyKind* pk = nullptr)
{
    return {name, MemberKind::Property, info, pk};
}

/**
 * @brief Retrieves the typed default value from a MemberDesc.
 * @tparam T The expected value type.
 * @param desc Member descriptor whose PropertyKind::getDefault to query.
 * @return The default value, or a value-initialized @p T if unavailable.
 */
template <class T>
T get_default_value(const MemberDesc& desc)
{
    T value{};
    const auto* pk = desc.propertyKind();
    if (pk && pk->getDefault) {
        if (const auto* any = pk->getDefault()) {
            any->get_data(&value, sizeof(T), type_uid<T>());
        }
    }
    return value;
}

/**
 * @brief Creates an Event MemberDesc.
 * @param name Member name for runtime lookup.
 * @param info Interface that declares this member (may be nullptr).
 */
constexpr MemberDesc EventDesc(string_view name, const InterfaceInfo* info = nullptr)
{
    return {name, MemberKind::Event, info};
}

/**
 * @brief Creates a Function MemberDesc.
 * @param name Member name for runtime lookup.
 * @param info Interface that declares this member (may be nullptr).
 * @param fk FunctionKind with trampoline and argument descriptors (may be nullptr).
 */
constexpr MemberDesc FunctionDesc(string_view name, const InterfaceInfo* info = nullptr,
                                  const FunctionKind* fk = nullptr)
{
    return {name, MemberKind::Function, info, fk};
}

} // namespace velk

#endif // VELK_MEMBER_DESC_H
