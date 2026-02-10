#ifndef INTF_STRATA_H
#define INTF_STRATA_H

#include <functional>
#include <string_view>

#include <interface/intf_object.h>
#include <interface/intf_object_factory.h>
#include <interface/intf_property.h>
#include <interface/types.h>

namespace strata {

/**
 * @brief Central interface for creating and managing Strata object types.
 *
 * Types are registered via IObjectFactory instances and can be created by UID.
 */
class IStrata : public Interface<IStrata>
{
public:
    using TypeCreateFn = std::function<IObject::Ptr()>;

    /** @brief Registers an object factory for the type it describes. */
    virtual ReturnValue RegisterType(const IObjectFactory &factory) = 0;
    /** @brief Unregisters a previously registered object factory. */
    virtual ReturnValue UnregisterType(const IObjectFactory &factory) = 0;
    /** @brief Creates an instance of a registered type by its UID. */
    virtual IInterface::Ptr Create(Uid uid) const = 0;
    /** @brief Returns the ClassInfo for a registered type, or nullptr if not found. */
    virtual const ClassInfo* GetClassInfo(Uid classUid) const = 0;
    /** @brief Creates a new IAny value container for the given type UID. */
    virtual IAny::Ptr CreateAny(Uid type) const = 0;
    /** @brief Creates a new property instance with the given type and optional initial value. */
    virtual IProperty::Ptr CreateProperty(Uid type, const IAny::Ptr& value) const = 0;
    /**
     * @brief Creates a property for type T with an optional initial value.
     * @tparam T The value type for the property.
     */
    template<class T>
    IProperty::Ptr CreateProperty(const IAny::Ptr& value = {}) const
    {
        return CreateProperty(TypeUid<T>(), value);
    }
    /**
     * @brief Creates an instance and casts it to the specified interface type.
     * @tparam T The target interface type.
     */
    template<class T>
    typename T::Ptr Create(Uid uid) const
    {
        return interface_pointer_cast<T>(Create(uid));
    }

    /**
     * @brief Registers a type using its static GetFactory() method.
     * @tparam T An Object-derived class with a static GetFactory() method.
     */
    template<class T>
    ReturnValue RegisterType()
    {
        return RegisterType(T::GetFactory());
    }
};

} // namespace strata

#endif // INTF_STRATA_H
