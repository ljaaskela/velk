#include "registry.h"
#include "event.h"
#include "function.h"
#include "property.h"
#include <ext/any.h>
#include <interface/types.h>
#include <iostream>

void RegisterTypes(IRegistry &registry)
{
    registry.RegisterType<PropertyImpl>();
    registry.RegisterType<EventImpl>();
    registry.RegisterType<FunctionImpl>();

    registry.RegisterType<SimpleAny<float>>();
    registry.RegisterType<SimpleAny<double>>();
    registry.RegisterType<SimpleAny<uint8_t>>();
    registry.RegisterType<SimpleAny<uint16_t>>();
    registry.RegisterType<SimpleAny<uint32_t>>();
    registry.RegisterType<SimpleAny<uint64_t>>();
    registry.RegisterType<SimpleAny<int8_t>>();
    registry.RegisterType<SimpleAny<int16_t>>();
    registry.RegisterType<SimpleAny<int32_t>>();
    registry.RegisterType<SimpleAny<int64_t>>();
    registry.RegisterType<SimpleAny<std::string>>();
}

Registry::Registry()
{
    RegisterTypes(*this);
}

ReturnValue Registry::RegisterType(const IObjectFactory &factory)
{
    auto &info = factory.GetClassInfo();
    std::cout << "Register " << info.name << " (uid: " << info.uid << ")" << std::endl;
    types_[info.uid] = &factory;
    return ReturnValue::SUCCESS;
}

ReturnValue Registry::UnregisterType(const IObjectFactory &factory)
{
    types_.erase(factory.GetClassInfo().uid);
    return ReturnValue::SUCCESS;
}

IInterface::Ptr Registry::Create(Uid uid) const
{
    if (auto fac = types_.find(uid); fac != types_.end()) {
        if (auto object = fac->second->CreateInstance()) {
            if (auto shared = object->GetInterface<ISharedFromObject>()) {
                shared->SetSelf(object);
            }
            return object;
        }
    }
    return {};
}

IAny::Ptr Registry::CreateAny(Uid type) const
{
    return interface_pointer_cast<IAny>(Create(type));
}

IProperty::Ptr Registry::CreateProperty(Uid type, const IAny::Ptr &value) const
{
    if (auto property = interface_pointer_cast<IProperty>(Create(ClassId::Property))) {
        if (auto pi = property->GetInterface<IPropertyInternal>()) {
            if (value && IsCompatible(value, type)) {
                if (pi->SetAny(value)) {
                    return property;
                }
                std::cerr << "Initial value is of incompatible type" << std::endl;
            }
            // Any was not specified for property instance, create new one
            if (auto any = CreateAny(type)) {
                if (pi->SetAny(any)) {
                    return property;
                }
            }
        }
    }
    return {};
}
