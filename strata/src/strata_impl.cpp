#include "strata_impl.h"
#include "function.h"
#include "metadata_container.h"
#include "property.h"
#include <ext/any.h>
#include <interface/types.h>
#include <iostream>

namespace strata {

void RegisterTypes(IStrata &strata)
{
    strata.register_type<PropertyImpl>();
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

const IObjectFactory* StrataImpl::find(Uid uid) const
{
    Entry key{uid, nullptr};
    auto it = std::lower_bound(types_.begin(), types_.end(), key);
    if (it != types_.end() && it->uid == uid) {
        return it->factory;
    }
    return nullptr;
}

ReturnValue StrataImpl::register_type(const IObjectFactory &factory)
{
    auto &info = factory.get_class_info();
    std::cout << "Register " << info.name << " (uid: " << info.uid << ")" << std::endl;
    Entry entry{info.uid, &factory};
    auto it = std::lower_bound(types_.begin(), types_.end(), entry);
    if (it != types_.end() && it->uid == info.uid) {
        it->factory = &factory;
    } else {
        types_.insert(it, entry);
    }
    return ReturnValue::SUCCESS;
}

ReturnValue StrataImpl::unregister_type(const IObjectFactory &factory)
{
    Entry key{factory.get_class_info().uid, nullptr};
    auto it = std::lower_bound(types_.begin(), types_.end(), key);
    if (it != types_.end() && it->uid == key.uid) {
        types_.erase(it);
    }
    return ReturnValue::SUCCESS;
}

IInterface::Ptr StrataImpl::create(Uid uid) const
{
    if (auto *factory = find(uid)) {
        if (auto object = factory->create_instance()) {
            if (auto shared = object->get_interface<ISharedFromObject>()) {
                // Object can provide shared_ptr to itself
                shared->set_self(object);
            }
            if (auto *meta = interface_cast<IMetadataContainer>(object)) {
                // Object can contain metadata
                auto &info = factory->get_class_info();
                // Object takes ownership of the MetadataContainer
                meta->set_metadata_container(new MetadataContainer(info.members, object.get()));
            }
            return object;
        }
    }
    return {};
}

const ClassInfo* StrataImpl::get_class_info(Uid classUid) const
{
    if (auto *factory = find(classUid)) {
        return &factory->get_class_info();
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

void StrataImpl::queue_deferred_tasks(array_view<DeferredTask> tasks) const
{
    std::lock_guard lock(deferred_mutex_);
    deferred_queue_.insert(deferred_queue_.end(), tasks.begin(), tasks.end());
}

void StrataImpl::update() const
{
    // Swap the queue under lock, then invoke outside the lock.
    // Tasks queued during invocation (by deferred handlers) will be picked up at the next update().
    std::vector<DeferredTask> tasks;
    {
        std::lock_guard lock(deferred_mutex_);
        tasks.swap(deferred_queue_);
    }
    for (auto &task : tasks) {
        task.fn->invoke(task.args.get());
    }
}

} // namespace strata
