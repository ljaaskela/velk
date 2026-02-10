#ifndef INTF_INTERFACE_H
#define INTF_INTERFACE_H

#include <common.h>

namespace strata {

/** @brief Static descriptor for an interface type, providing its UID and name. */
struct InterfaceInfo {
    Uid uid;
    std::string_view name;
};

/**
 * @brief Root interface for all Strata interfaces.
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
    using Ptr = std::shared_ptr<IInterface>;
    /** @brief Shared pointer to a const IInterface. */
    using ConstPtr = std::shared_ptr<const IInterface>;
    /** @brief Weak pointer to a mutable IInterface. */
    using WeakPtr = std::weak_ptr<IInterface>;
    /** @brief Weak pointer to a const IInterface. */
    using ConstWeakPtrr = std::weak_ptr<const IInterface>;

public:
    /** @brief Returns a pointer to the requested interface, or nullptr if not supported. */
    virtual IInterface* get_interface(Uid uid) = 0;
    /** @copydoc get_interface(Uid) */
    virtual const IInterface *get_interface(Uid uid) const = 0;

    /**
     * @brief Type-safe interface query.
     * @tparam T The interface type to query for. Must have a static UID member.
     */
    template<class T>
    T *get_interface() noexcept
    {
        return static_cast<T*>(get_interface(T::UID));
    }
    /** @copydoc get_interface() */
    template<class T>
    const T *get_interface() const noexcept
    {
        return static_cast<const T *>(get_interface(T::UID));
    }

    /** @brief Increments the reference count. */
    virtual void ref() = 0;
    /** @brief Decrements the reference count. May delete the object when it reaches zero. */
    virtual void unref() = 0;

protected:
    IInterface() = default;
    virtual ~IInterface() = default;
};

#include <interface/refcnt_ptr.h>

/**
 * @brief CRTP base template for defining concrete Strata interfaces.
 *
 * Provides automatic UID generation from the type name, and standard smart pointer
 * type aliases (Ptr, ConstPtr, WeakPtr, ConstWeakPtr, RefPtr).
 *
 * By default the UID is derived from the type name at compile time. To assign a
 * stable, user-defined UID instead, pass the high and low 64-bit halves as template
 * parameters. The @c STRATA_UID macro parses a UUID string into the two values:
 *
 * @code
 * // Auto-generated UID (default):
 * class IMyWidget : public Interface<IMyWidget> {};
 *
 * // User-specified UID:
 * class IMyWidget : public Interface<IMyWidget,
 *     STRATA_UID("cc262192-d151-941f-d542-d4c622b50b09")> {};
 * @endcode
 *
 * @tparam T The derived interface type (CRTP parameter).
 * @tparam UidHi High 64 bits of a user-specified UID (0 = auto-generate from type name).
 * @tparam UidLo Low 64 bits of a user-specified UID (0 = auto-generate from type name).
 */
template<typename T, uint64_t UidHi = 0, uint64_t UidLo = 0>
class Interface : public IInterface
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
    static constexpr InterfaceInfo INFO { compute_uid(), get_name<T>() };
    /** @brief Shared pointer to a mutable T. */
    using Ptr = std::shared_ptr<T>;
    /** @brief Shared pointer to a const T. */
    using ConstPtr = std::shared_ptr<const T>;
    /** @brief Weak pointer to a mutable T. */
    using WeakPtr = std::weak_ptr<T>;
    /** @brief Weak pointer to a const T. */
    using ConstWeakPtr = std::weak_ptr<const T>;
    /** @brief Intrusive reference-counting pointer to T. */
    using RefPtr = refcnt_ptr<T>;
    using IInterface::get_interface;

protected:
    Interface() = default;
    ~Interface() override = default;
};

} // namespace strata

#endif // INTF_INTERFACE_H
