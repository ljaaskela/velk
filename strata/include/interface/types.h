#ifndef TYPES_H
#define TYPES_H

#include <array_view.h>
#include <common.h>
#include <interface/intf_interface.h>
#include <type_traits>

namespace strata {

struct MemberDesc; // Forward declaration

/** @brief Describes a registered class with its UID, name, and static metadata. */
struct ClassInfo
{
    const Uid uid;
    const std::string_view name;
    const array_view<MemberDesc> members; // empty when no metadata
};

// Forward declarations for built-in implementation classes.
class PropertyImpl;
class FunctionImpl;

/** @brief Compile-time class identifiers for built-in object types. */
namespace ClassId {
/** @brief Default property object implementation. */
inline constexpr Uid Property = type_uid<PropertyImpl>();
/** @brief Default function object implementation. */
inline constexpr Uid Function = type_uid<FunctionImpl>();
/** @brief Default event object implementation (same as Function). */
inline constexpr Uid Event = Function;
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
    if (obj && obj->get_interface(T::UID)) {
        return std::dynamic_pointer_cast<T>(obj);
    }
    return nullptr;
}

/**
 * @brief Casts a shared const IInterface pointer to a derived const interface type.
 * @tparam T The target interface type.
 * @tparam U Deduced element type of the shared_ptr. Must be const-qualified (SFINAE).
 * @param obj The source pointer.
 * @return A shared_ptr<const T> if the interface is supported, nullptr otherwise.
 */
template<class T, class U, class = std::enable_if_t<std::is_const_v<U>>>
typename T::ConstPtr interface_pointer_cast(const std::shared_ptr<U> &obj)
{
    if (obj && obj->get_interface(T::UID)) {
        return std::dynamic_pointer_cast<const T>(obj);
    }
    return nullptr;
}

/**
 * @brief Casts an IInterface pointer to a derived interface type.
 * @tparam T The target interface type.
 * @param obj The source pointer.
 * @return A pointer to T if the interface is supported, nullptr otherwise.
 */
template <class T>
T *interface_cast(IInterface *obj)
{
    return obj ? obj->template get_interface<T>() : nullptr;
}

/** @copydoc interface_cast(IInterface*) */
template <class T>
const T *interface_cast(const IInterface *obj)
{
    return obj ? obj->template get_interface<T>() : nullptr;
}

/** @copydoc interface_cast(IInterface*) */
template <class T>
T *interface_cast(const IInterface::Ptr &obj)
{
    return obj ? obj->template get_interface<T>() : nullptr;
}

/**
 * @copydoc interface_cast(IInterface*)
 * @tparam U Deduced element type of the shared_ptr. Must be const-qualified (SFINAE).
 */
template<class T, class U, class = std::enable_if_t<std::is_const_v<U>>>
const T *interface_cast(const std::shared_ptr<U> &obj)
{
    return obj ? obj->template get_interface<T>() : nullptr;
}

/** @brief Standard return codes for Strata operations. Non-negative values indicate success. */
enum ReturnValue : int16_t {
    SUCCESS = 0,         ///< Operation succeeded.
    NOTHING_TO_DO = 1,   ///< Operation succeeded but had no effect (e.g. value unchanged).
    FAIL = -1,           ///< Operation failed.
    INVALID_ARGUMENT = -2, ///< One or more arguments were invalid.
};

/** @brief Returns true if the return value indicates success (non-negative). */
[[maybe_unused]] static constexpr bool succeeded(ReturnValue ret)
{
    return ret >= 0;
}

/** @brief Returns true if the return value indicates failure (negative). */
[[maybe_unused]] static constexpr bool failed(ReturnValue ret)
{
    return ret < 0;
}

} // namespace strata

#endif
