#ifndef EXT_INTERFACE_DISPATCH_H
#define EXT_INTERFACE_DISPATCH_H

#include <common.h>
#include <interface/intf_interface.h>

namespace strata {

/**
 * @brief Concrete implementation of IInterface for an arbitrary pack of interfaces.
 *
 * Inherits all Interfaces and dispatches get_interface queries by UID.
 * ref and unref are no-ops; override them in a derived class (e.g.
 * RefCountedDispatch) to add lifetime management.
 *
 * @tparam Interfaces The interfaces this class implements (each must derive from IInterface).
 */
template<class... Interfaces>
class InterfaceDispatch : public Interfaces...
{
private:
    /** @brief Walks the parent interface chain of T looking for a UID match. */
    template<class T>
    constexpr void check_parent_interface(Uid uid, void **interface)
    {
        if constexpr (!std::is_same_v<typename T::ParentInterface, IInterface>) {
            using Parent = typename T::ParentInterface;
            if (uid == Parent::UID) {
                *interface = static_cast<Parent *>(static_cast<T *>(this));
            } else {
                check_parent_interface<Parent>(uid, interface);
            }
        }
    }

    /** @brief Sets *interface to a T* pointer if uid matches T::UID or any parent UID. */
    template<class T>
    constexpr void find_interface(Uid uid, void **interface)
    {
        if (uid == T::UID) {
            *interface = static_cast<T *>(this);
        } else {
            check_parent_interface<T>(uid, interface);
        }
    }

    /** @brief Recursion terminator for find_siblings when the pack is empty. */
    template<class... B, class = std::enable_if_t<sizeof...(B) == 0>>
    constexpr void find_siblings(Uid, void **)
    {}
    /** @brief Walks the interface pack, calling find_interface for each type until a match is found. */
    template<class A, class... B>
    constexpr void find_siblings(Uid uid, void **interface)
    {
        find_interface<A>(uid, interface);
        if (*interface == nullptr) {
            find_siblings<B...>(uid, interface);
        }
    }

    /** @brief Returns an IInterface* via the first type in the pack (used for IInterface::UID queries). */
    template<class First, class...>
    constexpr IInterface *as_first_interface() { return static_cast<First *>(this); }

public:
    /**
     * @brief Resolves a UID to the corresponding interface pointer.
     *
     * For IInterface::UID, returns a pointer cast through the first interface in the pack.
     * For all other UIDs, walks the pack and returns the first match, or nullptr.
     */
    IInterface *get_interface(Uid uid) override
    {
        void *interface = nullptr;
        if (uid == IInterface::UID) {
            return as_first_interface<Interfaces...>();
        }
        find_siblings<Interfaces...>(uid, &interface);
        return static_cast<IInterface *>(interface);
    }
    /** @copydoc get_interface(Uid) */
    const IInterface *get_interface(Uid uid) const override
    {
        return const_cast<InterfaceDispatch *>(this)->get_interface(uid);
    }

    /** @brief Type-safe convenience wrapper; resolves T::UID and casts the result to T*. */
    template<class T>
    T *get_interface()
    {
        return static_cast<T *>(get_interface(T::UID));
    }

    /** @brief No-op. Override in a derived class to add reference counting. */
    void ref() override {}
    /** @brief No-op. Override in a derived class to add reference counting. */
    void unref() override {}

protected:
    InterfaceDispatch() = default;
    ~InterfaceDispatch() override = default;
};

} // namespace strata

#endif // EXT_INTERFACE_DISPATCH_H
