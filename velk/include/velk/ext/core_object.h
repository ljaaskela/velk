#ifndef VELK_EXT_CORE_OBJECT_H
#define VELK_EXT_CORE_OBJECT_H

#include <velk/common.h>
#include <velk/api/traits.h>
#include <velk/ext/interface_dispatch.h>
#include <velk/ext/refcounted_dispatch.h>
#include <velk/interface/intf_any.h>
#include <velk/interface/intf_object.h>
#include <velk/interface/intf_object_factory.h>
#include <velk/interface/intf_velk.h>

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

/** @brief Creates a new T and returns it cast to interface I. */
template<class T, class I>
typename I::Ptr make_object()
{
    return interface_pointer_cast<I>(make_object<T>());
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
 * @tparam FinalClass The concrete class whose class_id()/class_name() are used.
 */
template<class FinalClass>
class DefaultFactory : public ObjectFactory<FinalClass>
{
    const ClassInfo &get_class_info() const override
    {
        static constexpr ClassInfo info{
            FinalClass::class_id(),
            FinalClass::class_name(),
            FinalClass::class_interfaces
        };
        return info;
    }
};

/** @brief Declares a static constexpr class UID from a UUID string literal. */
#define VELK_CLASS_UID(str) static constexpr ::velk::Uid class_uid{str}

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
    static constexpr string_view class_name() noexcept { return ::velk::get_name<FinalClass>(); }
    /** @brief Returns the compile-time UID of FinalClass, or a user-specified UID if provided via class_uid. */
    static constexpr Uid class_id() noexcept
    {
        if constexpr (has_class_uid<FinalClass>::value) {
            return FinalClass::class_uid;
        } else {
            return type_uid<FinalClass>();
        }
    }

public: // IObject
    Uid get_class_uid() const override
    {
        return class_id();
    }

    string_view get_class_name() const override
    {
        return class_name();
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

#endif // VELK_EXT_CORE_OBJECT_H
