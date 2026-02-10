#ifndef EXT_INTERFACE_DISPATCH_H
#define EXT_INTERFACE_DISPATCH_H

#include <common.h>
#include <interface/intf_interface.h>

namespace strata {

/**
 * @brief Concrete implementation of IInterface for an arbitrary pack of interfaces.
 *
 * Inherits all Interfaces and dispatches GetInterface queries by UID.
 * Ref and UnRef are no-ops; override them in a derived class (e.g.
 * RefCountedDispatch) to add lifetime management.
 *
 * @tparam Interfaces The interfaces this class implements (each must derive from IInterface).
 */
template<class... Interfaces>
class InterfaceDispatch : public Interfaces...
{
private:
    /** @brief Sets *interface to a T* pointer if uid matches T::UID. */
    template<class T>
    constexpr void FindInterface(Uid uid, void **interface)
    {
        if (uid == T::UID) {
            *interface = static_cast<T *>(this);
        }
    }

    /** @brief Recursion terminator for FindSiblings when the pack is empty. */
    template<class... B, class = std::enable_if_t<sizeof...(B) == 0>>
    constexpr void FindSiblings(Uid, void **)
    {}
    /** @brief Walks the interface pack, calling FindInterface for each type until a match is found. */
    template<class A, class... B>
    constexpr void FindSiblings(Uid uid, void **interface)
    {
        FindInterface<A>(uid, interface);
        if (*interface == nullptr) {
            FindSiblings<B...>(uid, interface);
        }
    }

    /** @brief Returns an IInterface* via the first type in the pack (used for IInterface::UID queries). */
    template<class First, class...>
    constexpr IInterface *AsFirstInterface() { return static_cast<First *>(this); }

public:
    /**
     * @brief Resolves a UID to the corresponding interface pointer.
     *
     * For IInterface::UID, returns a pointer cast through the first interface in the pack.
     * For all other UIDs, walks the pack and returns the first match, or nullptr.
     */
    IInterface *GetInterface(Uid uid) override
    {
        void *interface = nullptr;
        if (uid == IInterface::UID) {
            return AsFirstInterface<Interfaces...>();
        }
        FindSiblings<Interfaces...>(uid, &interface);
        return static_cast<IInterface *>(interface);
    }
    /** @copydoc GetInterface(Uid) */
    const IInterface *GetInterface(Uid uid) const override
    {
        return const_cast<InterfaceDispatch *>(this)->GetInterface(uid);
    }

    /** @brief Type-safe convenience wrapper; resolves T::UID and casts the result to T*. */
    template<class T>
    T *GetInterface()
    {
        return static_cast<T *>(GetInterface(T::UID));
    }

    /** @brief No-op. Override in a derived class to add reference counting. */
    void Ref() override {}
    /** @brief No-op. Override in a derived class to add reference counting. */
    void UnRef() override {}

protected:
    InterfaceDispatch() = default;
    ~InterfaceDispatch() override = default;
};

} // namespace strata

#endif // EXT_INTERFACE_DISPATCH_H
