#ifndef VELK_TYPES_H
#define VELK_TYPES_H

#include <velk/array_view.h>
#include <velk/common.h>
#include <velk/interface/intf_interface.h>

namespace velk {

struct MemberDesc; // Forward declaration

/** @brief Describes a registered class with its UID, name, and static metadata. */
struct ClassInfo
{
    const Uid uid;                              ///< Unique identifier for this class.
    const string_view name;                     ///< Human-readable class name.
    const array_view<InterfaceInfo> interfaces; ///< Interfaces implemented by this class.
    const array_view<MemberDesc> members;       ///< Static metadata members (empty when no metadata).
};

/** @brief Compile-time class identifiers for built-in object types. */
namespace ClassId {
/** @brief Default property object implementation. */
inline constexpr Uid Property{"a66badbf-c750-4580-b035-b5446806d67e"};
/** @brief Default function object implementation. */
inline constexpr Uid Function{"d3c150cc-0b2b-4237-93c5-5a16e9619be8"};
/** @brief Default event object implementation (same as Function). */
inline constexpr Uid Event = Function;
/** @brief Default future object implementation. */
inline constexpr Uid Future{"371dfa91-1cf7-441e-b688-20d7e0114745"};
} // namespace ClassId

/** @brief A duration in microseconds. */
struct Duration
{
    int64_t us = 0; ///< Microseconds.

    /** @brief Constructs a Duration from seconds. */
    static constexpr Duration from_seconds(float s) { return {static_cast<int64_t>(s * 1'000'000.f)}; }
    /** @brief Constructs a Duration from milliseconds. */
    static constexpr Duration from_milliseconds(float ms) { return {static_cast<int64_t>(ms * 1'000.f)}; }

    /** @brief Converts to seconds. */
    constexpr float to_seconds() const { return static_cast<float>(us) / 1'000'000.f; }
    /** @brief Converts to milliseconds. */
    constexpr float to_milliseconds() const { return static_cast<float>(us) / 1'000.f; }
};

/** @brief Standard return codes for Velk operations. Non-negative values indicate success. */
enum ReturnValue : int16_t
{
    Success = 0,          ///< Operation succeeded.
    NothingToDo = 1,      ///< Operation succeeded but had no effect (e.g. value unchanged).
    Fail = -1,            ///< Operation failed.
    InvalidArgument = -2, ///< One or more arguments were invalid.
    ReadOnly = -3,        ///< Write rejected: target is read-only.
};

/** @brief General-purpose object flags. Checked by runtime implementations. */
namespace ObjectFlags {
inline constexpr uint32_t None = 0;
inline constexpr uint32_t ReadOnly = 1 << 0;    ///< Property rejects writes via set_value/set_data.
inline constexpr uint32_t HiveManaged = 1 << 1; ///< Object is managed by a Hive.
} // namespace ObjectFlags

/** @brief Returns true if the return value indicates success (non-negative). */
inline constexpr bool succeeded(ReturnValue ret)
{
    return ret >= 0;
}

/** @brief Returns true if the return value indicates failure (negative). */
inline constexpr bool failed(ReturnValue ret)
{
    return ret < 0;
}

} // namespace velk

#endif
