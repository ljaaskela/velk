#include "velk_impl.h"
#include "function.h"
#include "future.h"
#include "metadata_container.h"
#include "property.h"
#include <velk/ext/any.h>
#include <velk/interface/types.h>
#include <iostream>

namespace velk {

void RegisterTypes(IVelk &velk)
{
    velk.register_type<PropertyImpl>();
    velk.register_type<FunctionImpl>();
    velk.register_type<FutureImpl>();

    velk.register_type<ext::AnyValue<float>>();
    velk.register_type<ext::AnyValue<double>>();
    velk.register_type<ext::AnyValue<uint8_t>>();
    velk.register_type<ext::AnyValue<uint16_t>>();
    velk.register_type<ext::AnyValue<uint32_t>>();
    velk.register_type<ext::AnyValue<uint64_t>>();
    velk.register_type<ext::AnyValue<int8_t>>();
    velk.register_type<ext::AnyValue<int16_t>>();
    velk.register_type<ext::AnyValue<int32_t>>();
    velk.register_type<ext::AnyValue<int64_t>>();
    velk.register_type<ext::AnyValue<std::string>>();
}

VelkImpl::VelkImpl()
{
    RegisterTypes(*this);
}

const IObjectFactory* VelkImpl::find(Uid uid) const
{
    Entry key{uid, nullptr};
    auto it = std::lower_bound(types_.begin(), types_.end(), key);
    if (it != types_.end() && it->uid == uid) {
        return it->factory;
    }
    return nullptr;
}

ReturnValue VelkImpl::register_type(const IObjectFactory &factory)
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

ReturnValue VelkImpl::unregister_type(const IObjectFactory &factory)
{
    Entry key{factory.get_class_info().uid, nullptr};
    auto it = std::lower_bound(types_.begin(), types_.end(), key);
    if (it != types_.end() && it->uid == key.uid) {
        types_.erase(it);
    }
    return ReturnValue::SUCCESS;
}

IInterface::Ptr VelkImpl::create(Uid uid) const
{
    if (auto *factory = find(uid)) {
        if (auto object = factory->create_instance()) {
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

const ClassInfo* VelkImpl::get_class_info(Uid classUid) const
{
    if (auto *factory = find(classUid)) {
        return &factory->get_class_info();
    }
    return nullptr;
}

IAny::Ptr VelkImpl::create_any(Uid type) const
{
    return interface_pointer_cast<IAny>(create(type));
}

IProperty::Ptr VelkImpl::create_property(Uid type, const IAny::Ptr &value, int32_t flags) const
{
    if (auto property = interface_pointer_cast<IProperty>(create(ClassId::Property))) {
        if (auto pi = property->get_interface<IPropertyInternal>()) {
            pi->set_flags(flags);
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

void VelkImpl::queue_deferred_tasks(array_view<DeferredTask> tasks) const
{
    std::lock_guard lock(deferred_mutex_);
    deferred_queue_.insert(deferred_queue_.end(), tasks.begin(), tasks.end());
}

void VelkImpl::update() const
{
    // Swap the queue under lock, then invoke outside the lock.
    // Tasks queued during invocation (by deferred handlers) will be picked up at the next update().
    std::vector<DeferredTask> tasks;
    {
        std::lock_guard lock(deferred_mutex_);
        tasks.swap(deferred_queue_);
    }
    for (auto &task : tasks) {
        if (task.fn) {
            task.fn->invoke(task.args ? task.args->view() : FnArgs{});
        }
    }
}

IFuture::Ptr VelkImpl::create_future() const
{
    return interface_pointer_cast<IFuture>(create(ClassId::Future));
}

IFunction::Ptr VelkImpl::create_callback(IFunction::CallableFn* fn) const
{
    auto func = interface_pointer_cast<IFunction>(create(ClassId::Function));
    if (auto* internal = interface_cast<IFunctionInternal>(func); internal && fn) {
        internal->set_invoke_callback(fn);
    }
    return func;
}

IFunction::Ptr VelkImpl::create_owned_callback(void* context,
    IFunction::BoundFn* fn, IFunction::ContextDeleter* deleter) const
{
    auto func = interface_pointer_cast<IFunction>(create(ClassId::Function));
    if (auto* internal = interface_cast<IFunctionInternal>(func)) {
        internal->set_owned_callback(context, fn, deleter);
    }
    return func;
}

} // namespace velk
