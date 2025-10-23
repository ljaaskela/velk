#ifndef INTF_REGISTRY_H
#define INTF_REGISTRY_H

#include <functional>
#include <string_view>

#include <interface/intf_object.h>
#include <interface/intf_object_factory.h>
#include <interface/intf_property.h>
#include <interface/types.h>

class IRegistry : public IInterface
{
    INTERFACE(IRegistry)
public:
    using TypeCreateFn = std::function<IObject::Ptr()>;

    // Register a type
    virtual ReturnValue RegisterType(const IObjectFactory &factory) = 0;
    virtual ReturnValue UnregisterType(const IObjectFactory &factory) = 0;
    // Create an instance of previously registered type uid
    virtual IInterface::Ptr Create(Uid uid) const = 0;
    // Create a new any value of given type
    virtual IAny::Ptr CreateAny(Uid type) const = 0;
    // Create a new property instance with initial value.
    virtual IProperty::Ptr CreateProperty(Uid type, const IAny::Ptr& value) const = 0;
    // Create an instance of a property with a given type (which must have been registered earlier)
    template<class T>
    IProperty::Ptr CreateProperty(const IAny::Ptr& value = {}) const
    {
        return CreateProperty(TypeUid<T>(), value);
    }
    template<class T>
    typename T::Ptr Create(Uid uid) const
    {
        return interface_pointer_cast<T>(Create(uid));
    }
    template<class T>
    typename T::Ptr Create(const ClassInfo &info) const
    {
        return interface_pointer_cast<T>(Create(info.uid));
    }

    template<class T>
    ReturnValue RegisterType()
    {
        return RegisterType(T::GetFactory());
    }
};

[[maybe_unused]]
IRegistry& GetRegistry();



#endif // INTF_REGISTRY_H
