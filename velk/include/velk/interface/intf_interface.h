#ifndef VELK_INTF_INTERFACE_H
#define VELK_INTF_INTERFACE_H

#include <velk/common.h>
#include <velk/memory.h>

#include <type_traits>

namespace velk {

/** @brief Static descriptor for an interface type, providing its UID and name. */
struct InterfaceInfo
{
    Uid uid;
    string_view name;
};

/**
 * @brief Root interface for all Velk interfaces.
 *
 * Provides UID-based interface querying and manual reference counting.
 * All concrete interfaces derive from this through the Interface<T> CRTP template.
 */
class IInterface : NoCopyMove
{
public:
    /** @brief UID for the root interface (default-constructed, matches nothing specific). */
    static constexpr Uid UID{};
    /** @brief Shared pointer to a mutable IInterface. */
    using Ptr = shared_ptr<IInterface>;
    /** @brief Shared pointer to a const IInterface. */
    using ConstPtr = shared_ptr<const IInterface>;
    /** @brief Weak pointer to a mutable IInterface. */
    using WeakPtr = weak_ptr<IInterface>;
    /** @brief Weak pointer to a const IInterface. */
    using ConstWeakPtr = weak_ptr<const IInterface>;

public:
    /** @brief Returns a pointer to the requested interface, or nullptr if not supported. */
    virtual IInterface* get_interface(Uid uid) = 0;
    /** @copydoc get_interface(Uid) */
    virtual const IInterface* get_interface(Uid uid) const = 0;

    /**
     * @brief Type-safe interface query.
     * @tparam T The interface type to query for. Must have a static UID member.
     */
    template <class T>
    T* get_interface() noexcept
    {
        return static_cast<T*>(get_interface(T::UID));
    }
    /** @copydoc get_interface() */
    template <class T>
    const T* get_interface() const noexcept
    {
        return static_cast<const T*>(get_interface(T::UID));
    }

    /** @brief Increments the reference count. */
    virtual void ref() = 0;
    /** @brief Decrements the reference count. May delete the object when it reaches zero. */
    virtual void unref() = 0;

protected:
    IInterface() = default;
    virtual ~IInterface() = default;
};

#include <velk/interface/refcnt_ptr.h>

/**
 * @brief CRTP base template for defining concrete Velk interfaces.
 *
 * Provides automatic UID generation from the type name, and standard smart pointer
 * type aliases (Ptr, ConstPtr, WeakPtr, ConstWeakPtr, RefPtr).
 *
 * By default the UID is derived from the type name at compile time. To assign a
 * stable, user-defined UID instead, pass the high and low 64-bit halves as template
 * parameters. The @c VELK_UID macro parses a UUID string into the two values:
 *
 * @code
 * // Auto-generated UID (default):
 * class IMyWidget : public Interface<IMyWidget> {};
 *
 * // User-specified UID:
 * class IMyWidget : public Interface<IMyWidget,
 *     VELK_UID("cc262192-d151-941f-d542-d4c622b50b09")> {};
 * @endcode
 *
 * @tparam T The derived interface type (CRTP parameter).
 * @tparam Base The parent interface to inherit from (default: IInterface).
 * @tparam UidHi High 64 bits of a user-specified UID (0 = auto-generate from type name).
 * @tparam UidLo Low 64 bits of a user-specified UID (0 = auto-generate from type name).
 */
template <typename T, typename Base = IInterface, uint64_t UidHi = 0, uint64_t UidLo = 0>
class Interface : public Base
{
    static constexpr Uid compute_uid()
    {
        if constexpr (UidHi == 0 && UidLo == 0) {
            return type_uid<T>();
        } else {
            return Uid(UidHi, UidLo);
        }
    }

public:
    /** @brief Compile-time unique identifier for this interface type. */
    static constexpr Uid UID = compute_uid();
    /** @brief Static descriptor containing the UID and human-readable name. */
    static constexpr InterfaceInfo INFO{compute_uid(), get_name<T>()};
    /** @brief Shared pointer to a mutable T. */
    using Ptr = shared_ptr<T>;
    /** @brief Shared pointer to a const T. */
    using ConstPtr = shared_ptr<const T>;
    /** @brief Weak pointer to a mutable T. */
    using WeakPtr = weak_ptr<T>;
    /** @brief Weak pointer to a const T. */
    using ConstWeakPtr = weak_ptr<const T>;
    /** @brief Intrusive reference-counting pointer to T. */
    using RefPtr = refcnt_ptr<T>;
    /** @brief The parent interface type (Base), used for interface dispatch walking. */
    using ParentInterface = Base;
    using IInterface::get_interface;

protected:
    /** @brief Alias for the derived interface type, used by VELK_INTERFACE trampoline macros. */
    using _velk_intf_type = T;
    Interface() = default;
    ~Interface() override = default;
};

/**
 * @brief Casts a shared IInterface pointer to a derived interface type.
 * @tparam T The target interface type.
 * @param obj The source pointer.
 * @return A shared_ptr<T> if the interface is supported, nullptr otherwise.
 */
template <class T>
typename T::Ptr interface_pointer_cast(const IInterface::Ptr& obj)
{
    if (auto* p = obj ? obj->template get_interface<T>() : nullptr) {
        return typename T::Ptr(p, obj.block());
    }
    return {};
}

/**
 * @brief Casts a shared const IInterface pointer to a derived const interface type.
 * @tparam T The target interface type.
 * @tparam U Deduced element type of the shared_ptr. Must be const-qualified (SFINAE).
 * @param obj The source pointer.
 * @return A shared_ptr<const T> if the interface is supported, nullptr otherwise.
 */
template <class T, class U, class = std::enable_if_t<std::is_const_v<U>>>
typename T::ConstPtr interface_pointer_cast(const shared_ptr<U>& obj)
{
    if (auto* p = obj ? obj->template get_interface<T>() : nullptr) {
        return typename T::ConstPtr(p, obj.block());
    }
    return {};
}

/**
 * @brief Casts an IInterface pointer to a derived interface type.
 * @tparam T The target interface type.
 * @param obj The source pointer.
 * @return A pointer to T if the interface is supported, nullptr otherwise.
 */
template <class T>
T* interface_cast(IInterface* obj)
{
    return obj ? obj->template get_interface<T>() : nullptr;
}

/** @copydoc interface_cast(IInterface*) */
template <class T>
const T* interface_cast(const IInterface* obj)
{
    return obj ? obj->template get_interface<T>() : nullptr;
}

/** @copydoc interface_cast(IInterface*) */
template <class T>
T* interface_cast(const IInterface::Ptr& obj)
{
    return obj ? obj->template get_interface<T>() : nullptr;
}

/**
 * @copydoc interface_cast(IInterface*)
 * @tparam U Deduced element type of the shared_ptr. Must be const-qualified (SFINAE).
 */
template <class T, class U, class = std::enable_if_t<std::is_const_v<U>>>
const T* interface_cast(const shared_ptr<U>& obj)
{
    return obj ? obj->template get_interface<T>() : nullptr;
}

} // namespace velk

#endif // VELK_INTF_INTERFACE_H
