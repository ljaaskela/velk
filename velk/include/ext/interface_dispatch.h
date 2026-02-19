#ifndef EXT_INTERFACE_DISPATCH_H
#define EXT_INTERFACE_DISPATCH_H

#include <array_view.h>
#include <common.h>
#include <interface/intf_interface.h>
#include <type_traits>

namespace velk::ext {

/**
 * @brief Concrete implementation of IInterface for an arbitrary pack of interfaces.
 *
 * Inherits all Interfaces and dispatches get_interface queries by UID.
 * At compile time, builds a flat table of all reachable interfaces (including parents)
 * with corresponding cast function pointers. At runtime, get_interface() is a simple
 * linear scan of this table.
 *
 * ref and unref are no-ops; override them in a derived class (e.g.
 * RefCountedDispatch) to add lifetime management.
 *
 * @tparam Interfaces The interfaces this class implements (each must derive from IInterface).
 */
template<class... Interfaces>
class InterfaceDispatch : public Interfaces...
{
private:
    using Self = InterfaceDispatch;

    /** @brief Function pointer type for casting Self* to a specific IInterface*. */
    using CastFn = IInterface* (*)(Self*);

    /**
     * @brief Returns the number of interfaces in T's parent chain (excluding IInterface).
     * @tparam T The interface to measure.
     */
    template<class T>
    static constexpr size_t chain_length()
    {
        if constexpr (std::is_same_v<T, IInterface>) return 0;
        else return 1 + chain_length<typename T::ParentInterface>();
    }

    /** @brief Upper bound on total interface count (sum of all chain lengths before dedup). */
    static constexpr size_t max_interface_count_ = (0 + ... + chain_length<Interfaces>());

    /**
     * @brief Casts Self* to a target interface via a root pack member.
     *
     * The double static_cast ensures correct MI pointer adjustment: first to Root
     * (which Self directly inherits), then to Target (Root itself or one of its parents).
     *
     * @tparam Root   A direct base of Self (one of Interfaces...).
     * @tparam Target Root or one of Root's ancestor interfaces.
     */
    template<class Root, class Target>
    static IInterface* cast_to(Self* self)
    {
        return static_cast<Target*>(static_cast<Root*>(self));
    }

    /**
     * @brief Compile-time storage for the flat interface dispatch table.
     *
     * Holds parallel arrays of InterfaceInfo (uid + name) and CastFn pointers.
     * Built by make_interface_list() via fold expression over the interface pack.
     * Entries are deduplicated by UID (first occurrence wins).
     */
    struct InterfaceListData
    {
        static constexpr size_t interface_list_size_ = max_interface_count_ > 0 ? max_interface_count_ : 1;
        InterfaceInfo entries[interface_list_size_]{};
        CastFn casts[interface_list_size_]{};
        size_t count{0};

        /** @brief Returns true if the given UID is already in the list. */
        constexpr bool contains(Uid uid) const
        {
            for (size_t i = 0; i < count; ++i) {
                if (entries[i].uid == uid) return true;
            }
            return false;
        }

        /** @brief Appends an entry if its UID is not already present. */
        constexpr void add(InterfaceInfo info, CastFn cast)
        {
            if (!contains(info.uid)) {
                entries[count] = info;
                casts[count] = cast;
                count++;
            }
        }
    };

    /**
     * @brief Recursively adds T and all its parents (excluding IInterface) to the list.
     *
     * All casts route through Root (the top-level pack member) to ensure correct
     * MI pointer adjustment.
     *
     * @tparam Root The direct base of Self that owns this parent chain.
     * @tparam T    The current interface in the chain (Root, then Root::ParentInterface, etc.).
     */
    template<class Root, class T>
    static constexpr void collect_chain(InterfaceListData& list)
    {
        if constexpr (!std::is_same_v<T, IInterface>) {
            list.add(T::INFO, &cast_to<Root, T>);
            collect_chain<Root, typename T::ParentInterface>(list);
        }
    }

    /** @brief Builds the complete interface table by collecting chains from all pack members. */
    static constexpr InterfaceListData make_interface_list()
    {
        InterfaceListData list{};
        (collect_chain<Interfaces, Interfaces>(list), ...);
        return list;
    }

    /** @brief The compile-time interface dispatch table. */
    static constexpr InterfaceListData class_interface_data_ = make_interface_list();

public:
    /**
     * @brief Resolves a UID to the corresponding interface pointer.
     *
     * For IInterface::UID, returns this (every object implements IInterface).
     * For all other UIDs, scans the flat interface table and invokes the
     * matching cast function, or returns nullptr if not found.
     */
    IInterface *get_interface(Uid uid) override
    {
        if (uid == IInterface::UID) {
            return static_cast<IInterface*>(static_cast<void*>(this));
        }
        for (size_t i = 0; i < class_interface_data_.count; ++i) {
            if (class_interface_data_.entries[i].uid == uid) {
                return class_interface_data_.casts[i](this);
            }
        }
        return nullptr;
    }
    /** @copydoc get_interface(Uid) */
    const IInterface *get_interface(Uid uid) const override
    {
        return const_cast<InterfaceDispatch *>(this)->get_interface(uid);
    }

    /** @brief Compile-time list of all interfaces implemented by this class (including parents). */
    static constexpr array_view<InterfaceInfo> class_interfaces{
        class_interface_data_.entries, class_interface_data_.count
    };

    /**
     * @brief Type-safe convenience wrapper for get_interface.
     * @tparam T The target interface type. Must have a static UID member.
     * @return T* if this object implements T, nullptr otherwise.
     */
    template<class T>
    T *get_interface()
    {
        return static_cast<T *>(get_interface(T::UID));
    }

    /**
     * @brief No-op. Override in a derived class to add reference counting.
     *
     * @warning Objects that use no-op ref/unref must not be wrapped in
     * velk::shared_ptr, as the pointer will call unref() on release which
     * does nothing, silently leaking the object. Use RefCountedDispatch
     * (or implement ref/unref yourself) for shared_ptr-managed objects.
     */
    void ref() override {}
    /** @copydoc ref() */
    void unref() override {}

protected:
    InterfaceDispatch() = default;
    ~InterfaceDispatch() override = default;
};

} // namespace velk::ext

#endif // EXT_INTERFACE_DISPATCH_H
