#ifndef EXT_CORE_OBJECT_H
#define EXT_CORE_OBJECT_H

#include <common.h>
#include <ext/interface_dispatch.h>
#include <ext/refcounted_dispatch.h>
#include <interface/intf_any.h>
#include <interface/intf_object.h>
#include <interface/intf_object_factory.h>
#include <interface/intf_velk.h>

namespace velk::ext {

/**
 * @brief Default IObjectFactory implementation that creates instances of FinalClass.
 *
 * Created objects are wrapped in a shared_ptr with a custom deleter that calls unref().
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
        // Custom deleter for factory-created shared_ptrs which just decrease refcount
        return std::shared_ptr<FinalClass>(new FinalClass, [](FinalClass *p) { p->unref(); });
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
            FinalClass::get_class_name(),
            FinalClass::class_interfaces
        };
        return info;
    }
};

/**
 * @brief CRTP base for concrete Velk objects (without metadata).
 *
 * Provides automatic class name/UID generation, self-pointer management,
 * and a static factory for Velk integration.
 *
 * @tparam FinalClass The final derived class (CRTP parameter).
 * @tparam Interfaces Additional interfaces the object implements.
 */
template<class FinalClass, class... Interfaces>
class ObjectCore : public RefCountedDispatch<ISharedFromObject, Interfaces...>
{
    template<class T, class = void>
    struct has_class_uid : std::false_type {};
    template<class T>
    struct has_class_uid<T, std::void_t<decltype(T::class_uid)>> : std::true_type {};

public:
    ObjectCore() = default;
    ~ObjectCore() override = default;

public:
    /** @brief Compile-time list of all interfaces implemented by this class. */
    static constexpr InterfaceInfo class_interfaces_[] = {
        ISharedFromObject::INFO, Interfaces::INFO...
    };
    static constexpr array_view<InterfaceInfo> class_interfaces{class_interfaces_, 1 + sizeof...(Interfaces)};

    /** @brief Returns the compile-time class name of FinalClass. */
    static constexpr string_view get_class_name() noexcept { return get_name<FinalClass>(); }
    /** @brief Returns the compile-time UID of FinalClass, or a user-specified UID if provided via class_uid. */
    static constexpr Uid get_class_uid() noexcept
    {
        if constexpr (has_class_uid<FinalClass>::value) {
            return FinalClass::class_uid;
        } else {
            return type_uid<FinalClass>();
        }
    }

    /** @brief Stores a weak self-reference (called once by the factory). */
    void set_self(const IObject::Ptr &self) override
    {
        if (self_.expired()) { // Only allow one set (called by factory)
            self_ = self;
        }
    }
    /** @brief Returns a shared_ptr to this object, or nullptr if expired. */
    IObject::Ptr get_self() const override { return self_.lock(); }

    template<class T>
    typename T::Ptr get_self() const
    {
        return interface_pointer_cast<T>(self_.lock());
    }

public:
    /** @brief Returns the singleton factory for creating instances of FinalClass. */
    static const IObjectFactory &get_factory()
    {
        static DefaultFactory<FinalClass> factory_;
        return factory_;
    }

private:
    IObject::WeakPtr self_{};
};

} // namespace velk::ext

#endif // EXT_CORE_OBJECT_H
