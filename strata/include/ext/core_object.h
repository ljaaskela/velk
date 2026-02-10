#ifndef EXT_CORE_OBJECT_H
#define EXT_CORE_OBJECT_H

#include <common.h>
#include <ext/interface_dispatch.h>
#include <ext/refcounted_dispatch.h>
#include <interface/intf_any.h>
#include <interface/intf_object.h>
#include <interface/intf_object_factory.h>
#include <interface/intf_strata.h>

namespace strata {

/**
 * @brief Default IObjectFactory implementation that creates instances of FinalClass.
 *
 * Created objects are wrapped in a shared_ptr with a custom deleter that calls UnRef().
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
    IObject::Ptr CreateInstance() const override
    {
        // Custom deleter for factory-created shared_ptrs which just decrease refcount
        return std::shared_ptr<FinalClass>(new FinalClass, [](FinalClass *p) { p->UnRef(); });
    }
};

/**
 * @brief CRTP base for concrete Strata objects (without metadata).
 *
 * Provides automatic class name/UID generation, self-pointer management,
 * and a static factory for Strata integration.
 *
 * @tparam FinalClass The final derived class (CRTP parameter).
 * @tparam Interfaces Additional interfaces the object implements.
 */
template<class FinalClass, class... Interfaces>
class CoreObject : public RefCountedDispatch<IObject, ISharedFromObject, Interfaces...>
{
public:
    CoreObject() = default;
    ~CoreObject() override = default;

public:
    /** @brief Returns the compile-time class name of FinalClass. */
    static constexpr std::string_view GetClassName() noexcept { return GetName<FinalClass>(); }
    /** @brief Returns the compile-time UID of FinalClass. */
    static constexpr Uid GetClassUid() noexcept { return TypeUid<FinalClass>(); }

    /** @brief Stores a weak self-reference (called once by the factory). */
    void SetSelf(const IObject::Ptr &self) override
    {
        if (self_.expired()) { // Only allow one set (called by factory)
            self_ = self;
        }
    }
    /** @brief Returns a shared_ptr to this object, or nullptr if expired. */
    IObject::Ptr GetSelf() const override { return self_.lock(); }

    template<class T>
    typename T::Ptr GetSelf() const
    {
        return interface_pointer_cast<T>(self_.lock());
    }

public:
    /** @brief Returns the singleton factory for creating instances of FinalClass. */
    static const IObjectFactory &GetFactory()
    {
        static Factory factory_;
        return factory_;
    }

private:
    IObject::WeakPtr self_{};

    class Factory : public ObjectFactory<FinalClass>
    {
        const ClassInfo &GetClassInfo() const override
        {
            static constexpr ClassInfo info{FinalClass::GetClassUid(), FinalClass::GetClassName()};
            return info;
        }
    };
};

} // namespace strata

#endif // EXT_CORE_OBJECT_H
