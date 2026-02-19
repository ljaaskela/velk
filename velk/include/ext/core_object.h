#ifndef EXT_CORE_OBJECT_H
#define EXT_CORE_OBJECT_H

#include <common.h>
#include <api/traits.h>
#include <ext/interface_dispatch.h>
#include <ext/refcounted_dispatch.h>
#include <interface/intf_any.h>
#include <interface/intf_object.h>
#include <interface/intf_object_factory.h>
#include <interface/intf_velk.h>

namespace velk::ext {

/** @brief Creates a new T, sets self-pointer, and wraps it in a shared_ptr. */
template<class T>
IObject::Ptr make_object()
{
    auto* obj = new T;
    IObject::Ptr result(static_cast<IObject*>(obj), obj->get_block(), adopt_ref);
    auto* block = obj->get_block();
    if (block && !block->ptr) {
        block->ptr = result.get();
    }
    return result;
}

/**
 * @brief Default IObjectFactory implementation that creates instances of FinalClass.
 *
 * Created objects are wrapped in a shared_ptr that calls unref() on release.
 *
 * @tparam FinalClass The concrete class to instantiate.
 */
template<class FinalClass>
class ObjectFactory : public InterfaceDispatch<IObjectFactory>
{
public:
    ObjectFactory() = default;
    ~ObjectFactory() override = default;

public:
    IObject::Ptr create_instance() const override
    {
        return make_object<FinalClass>();
    }
};

/**
 * @brief ObjectFactory with a default ClassInfo (UID + name, no metadata members).
 *
 * Used by ObjectCore and AnyBase where no compile-time metadata array is needed.
 *
 * @tparam FinalClass The concrete class whose get_class_uid()/get_class_name() are used.
 */
template<class FinalClass>
class DefaultFactory : public ObjectFactory<FinalClass>
{
    const ClassInfo &get_class_info() const override
    {
        static constexpr ClassInfo info{
            FinalClass::get_class_uid(),
            FinalClass::get_static_class_name(),
            FinalClass::class_interfaces
        };
        return info;
    }
};

// IObject detection: walks ParentInterface chains to check if IObject is already reachable.

/** @brief Selects the RefCountedDispatch base, prepending IObject only if not already reachable. */
template<bool HasIObject, class... Interfaces>
struct ObjectCoreBase { using type = RefCountedDispatch<IObject, Interfaces...>; };

template<class... Interfaces>
struct ObjectCoreBase<true, Interfaces...> { using type = RefCountedDispatch<Interfaces...>; };

/**
 * @brief CRTP base for concrete Velk objects (without metadata).
 *
 * Provides automatic class name/UID generation, self-pointer management,
 * and a static factory for Velk integration. IObject is added to the
 * interface pack only if not already reachable through the Interfaces.
 *
 * @tparam FinalClass The final derived class (CRTP parameter).
 * @tparam Interfaces Additional interfaces the object implements.
 */
template<class FinalClass, class... Interfaces>
class ObjectCore : public ObjectCoreBase<
    (detail::has_iobject_in_chain<Interfaces>() || ...), Interfaces...>::type
{
    template<class T, class = void>
    struct has_class_uid : std::false_type {};
    template<class T>
    struct has_class_uid<T, std::void_t<decltype(T::class_uid)>> : std::true_type {};

public:
    ObjectCore() = default;
    ~ObjectCore() override = default;

public:
    /** @brief Returns the compile-time class name of FinalClass. */
    static constexpr string_view get_static_class_name() noexcept { return get_name<FinalClass>(); }
    /** @brief Returns the compile-time UID of FinalClass, or a user-specified UID if provided via class_uid. */
    static constexpr Uid get_class_uid() noexcept
    {
        if constexpr (has_class_uid<FinalClass>::value) {
            return FinalClass::class_uid;
        } else {
            return type_uid<FinalClass>();
        }
    }

public: // IObject
    string_view get_class_name() const override
    {
        return get_static_class_name();
    }

    /** @brief Returns a shared_ptr to this object, or empty if expired. */
    IObject::Ptr get_self() const override
    {
        auto* block = this->get_block();
        // No self set, or object already destroyed (strong count reached zero) -> return {}
        return block && block->ptr && block->strong.load(std::memory_order_acquire) ?
            IObject::Ptr(static_cast<IObject*>(block->ptr), block) : nullptr;
    }

    template<class T>
    typename T::Ptr get_self() const
    {
        return interface_pointer_cast<T>(get_self());
    }

public:
    /** @brief Returns the singleton factory for creating instances of FinalClass. */
    static const IObjectFactory &get_factory()
    {
        static DefaultFactory<FinalClass> factory_;
        return factory_;
    }
};

} // namespace velk::ext

#endif // EXT_CORE_OBJECT_H
