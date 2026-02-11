#include "metadata_container.h"
#include "strata_impl.h"
#include "event.h"
#include "function.h"
#include "property.h"
#include <ext/any.h>
#include <interface/types.h>
#include <iostream>

namespace strata {

void RegisterTypes(IStrata &strata)
{
    strata.register_type<PropertyImpl>();
    strata.register_type<EventImpl>();
    strata.register_type<FunctionImpl>();

    strata.register_type<SimpleAny<float>>();
    strata.register_type<SimpleAny<double>>();
    strata.register_type<SimpleAny<uint8_t>>();
    strata.register_type<SimpleAny<uint16_t>>();
    strata.register_type<SimpleAny<uint32_t>>();
    strata.register_type<SimpleAny<uint64_t>>();
    strata.register_type<SimpleAny<int8_t>>();
    strata.register_type<SimpleAny<int16_t>>();
    strata.register_type<SimpleAny<int32_t>>();
    strata.register_type<SimpleAny<int64_t>>();
    strata.register_type<SimpleAny<std::string>>();
}

StrataImpl::StrataImpl()
{
    RegisterTypes(*this);
}

ReturnValue StrataImpl::register_type(const IObjectFactory &factory)
{
    auto &info = factory.get_class_info();
    std::cout << "Register " << info.name << " (uid: " << info.uid << ")" << std::endl;
    types_[info.uid] = &factory;
    return ReturnValue::SUCCESS;
}

ReturnValue StrataImpl::unregister_type(const IObjectFactory &factory)
{
    types_.erase(factory.get_class_info().uid);
    return ReturnValue::SUCCESS;
}

IInterface::Ptr StrataImpl::create(Uid uid) const
{
    if (auto fac = types_.find(uid); fac != types_.end()) {
        if (auto object = fac->second->create_instance()) {
            if (auto shared = object->get_interface<ISharedFromObject>()) {
                shared->set_self(object);
            }
            auto& info = fac->second->get_class_info();
            if (!info.members.empty()) {
                if (auto *meta = interface_cast<IMetadataContainer>(object)) {
                    // Object takes ownership
                    meta->set_metadata_container(new MetadataContainer(info.members, *this, object.get()));
                }
            }
            return object;
        }
    }
    return {};
}

const ClassInfo* StrataImpl::get_class_info(Uid classUid) const
{
    if (auto fac = types_.find(classUid); fac != types_.end()) {
        return &fac->second->get_class_info();
    }
    return nullptr;
}

IAny::Ptr StrataImpl::create_any(Uid type) const
{
    return interface_pointer_cast<IAny>(create(type));
}

IProperty::Ptr StrataImpl::create_property(Uid type, const IAny::Ptr &value) const
{
    if (auto property = interface_pointer_cast<IProperty>(create(ClassId::Property))) {
        if (auto pi = property->get_interface<IPropertyInternal>()) {
            if (value && is_compatible(value, type)) {
                if (pi->set_any(value)) {
                    return property;
                }
                std::cerr << "Initial value is of incompatible type" << std::endl;
            }
            // Any was not specified for property instance, create new one
            if (auto any = create_any(type)) {
                if (pi->set_any(any)) {
                    return property;
                }
            }
        }
    }
    return {};
}

} // namespace strata
