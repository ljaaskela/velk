#ifndef TYPES_H
#define TYPES_H

#include <common.h>
#include <interface/intf_interface.h>

/** @brief Describes a registered class with its UID and human-readable name. */
struct ClassInfo
{
    const Uid uid;
    const std::string_view name;
};

// Forward declarations for built-in implementation classes.
class PropertyImpl;
class EventImpl;
class FunctionImpl;

/** @brief Compile-time class identifiers for built-in object types. */
namespace ClassId {
    inline constexpr Uid Property = TypeUid<PropertyImpl>();
    inline constexpr Uid Event = TypeUid<EventImpl>();
    inline constexpr Uid Function = TypeUid<FunctionImpl>();
}

/**
 * @brief Casts a shared IInterface pointer to a derived interface type.
 * @tparam T The target interface type.
 * @param obj The source pointer.
 * @return A shared_ptr<T> if the interface is supported, nullptr otherwise.
 */
template <class T>
typename T::Ptr interface_pointer_cast(const IInterface::Ptr& obj)
{
    if (obj && obj->GetInterface(T::UID)) {
        return std::dynamic_pointer_cast<T>(obj);
    }
    return nullptr;
}

/** @brief Standard return codes for LTK operations. Non-negative values indicate success. */
enum ReturnValue : int16_t {
    SUCCESS = 0,         ///< Operation succeeded.
    NOTHING_TO_DO = 1,   ///< Operation succeeded but had no effect (e.g. value unchanged).
    FAIL = -1,           ///< Operation failed.
    INVALID_ARGUMENT = -2, ///< One or more arguments were invalid.
};

/** @brief Returns true if the return value indicates success (non-negative). */
[[maybe_unused]] static constexpr bool Succeeded(ReturnValue ret)
{
    return ret >= 0;
}

/** @brief Returns true if the return value indicates failure (negative). */
[[maybe_unused]] static constexpr bool Failed(ReturnValue ret)
{
    return ret < 0;
}

#endif
