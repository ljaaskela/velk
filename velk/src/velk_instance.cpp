#include "velk_instance.h"

#include "function.h"

#include <velk/interface/types.h>

namespace velk {

VelkInstance::VelkInstance() : type_registry_(*this), plugin_registry_(*this, type_registry_) {}

VelkInstance::~VelkInstance()
{
    plugin_registry_.shutdown_all();
}

ILog& get_logger(const VelkInstance& instance)
{
    return static_cast<ILog&>(*const_cast<VelkInstance*>(&instance));
}

IInterface::Ptr VelkInstance::create(Uid uid) const
{
    return type_registry_.create(uid);
}

IAny::Ptr VelkInstance::create_any(Uid type) const
{
    return interface_pointer_cast<IAny>(create(type));
}

IProperty::Ptr VelkInstance::create_property(Uid type, const IAny::Ptr& value, int32_t flags) const
{
    auto property = interface_pointer_cast<IProperty>(create(ClassId::Property));
    if (auto pi = interface_cast<IPropertyInternal>(property)) {
        pi->set_flags(flags);
        if (value && is_compatible(value, type)) {
            if (pi->set_any(value)) {
                return property;
            }
            detail::velk_log(get_logger(*this),
                             LogLevel::Error,
                             __FILE__,
                             __LINE__,
                             "Initial value is of incompatible type");
        }
        // Any was not specified for property instance, create new one
        if (auto any = create_any(type)) {
            if (pi->set_any(any)) {
                return property;
            }
        }
    }
    return {};
}

void VelkInstance::queue_deferred_tasks(array_view<DeferredTask> tasks) const
{
    std::lock_guard lock(deferred_mutex_);
    deferred_queue_.insert(deferred_queue_.end(), tasks.begin(), tasks.end());
}

void VelkInstance::queue_deferred_property(DeferredPropertySet task) const
{
    std::lock_guard lock(deferred_mutex_);
    deferred_property_queue_.push_back(std::move(task));
}

void VelkInstance::flush_deferred_properties(std::vector<DeferredPropertySet>& propSets) const
{
    // Coalesce property sets: walk backwards, lock each weak_ptr once, keep last-write-wins.
    struct CoalescedEntry
    {
        IPropertyInternal::Ptr property;
        IAny* value;
    };
    std::vector<CoalescedEntry> unique;
    for (auto it = propSets.rbegin(); it != propSets.rend(); ++it) {
        auto locked = it->property.lock();
        if (!locked) {
            continue;
        }
        bool found = false;
        for (auto& u : unique) {
            if (u.property == locked) {
                found = true;
                break;
            }
        }
        if (!found) {
            unique.push_back({std::move(locked), it->value.get()});
        }
    }
    // First pass: apply all values silently in original queue order, collect those needing notification.
    std::vector<IPropertyInternal*> notify;
    for (auto it = unique.rbegin(); it != unique.rend(); ++it) {
        if (it->property->set_value_silent(*it->value) == ReturnValue::Success) {
            notify.push_back(it->property.get());
        }
    }
    // Second pass: fire on_changed for all properties that changed.
    for (auto* prop : notify) {
        invoke_event(prop->on_changed(), prop->get_any().get());
    }
}

void VelkInstance::update(Duration time) const
{
    // Swap the queues under lock, then invoke outside the lock.
    // Tasks queued during invocation (by deferred handlers) will be picked up at the next update().
    std::vector<DeferredTask> tasks;
    std::vector<DeferredPropertySet> propSets;
    {
        std::lock_guard lock(deferred_mutex_);
        tasks.swap(deferred_queue_);
        propSets.swap(deferred_property_queue_);
    }

    // Run deferred tasks
    for (auto& task : tasks) {
        if (task.fn) {
            task.fn->invoke(task.args ? task.args->view() : FnArgs{});
        }
    }

    // Set deferred properties
    if (!propSets.empty()) {
        flush_deferred_properties(propSets);
    }

    // Update plugins that have asked for updates
    plugin_registry_.notify_plugins(time);
}

IFuture::Ptr VelkInstance::create_future() const
{
    return interface_pointer_cast<IFuture>(create(ClassId::Future));
}

IFunction::Ptr VelkInstance::create_callback(IFunction::CallableFn* fn) const
{
    auto func = interface_pointer_cast<IFunction>(create(ClassId::Function));
    if (fn) {
        if (auto* internal = interface_cast<IFunctionInternal>(func)) {
            internal->set_invoke_callback(fn);
        }
    }
    return func;
}

IFunction::Ptr VelkInstance::create_owned_callback(void* context, IFunction::BoundFn* fn,
                                                   IFunction::ContextDeleter* deleter) const
{
    auto func = interface_pointer_cast<IFunction>(create(ClassId::Function));
    if (fn && deleter) {
        if (auto* internal = interface_cast<IFunctionInternal>(func)) {
            internal->set_owned_callback(context, fn, deleter);
        }
    }
    return func;
}

void VelkInstance::set_sink(const ILogSink::Ptr& sink)
{
    sink_ = sink;
}

void VelkInstance::set_level(LogLevel level)
{
    level_ = level;
}

LogLevel VelkInstance::get_level() const
{
    return level_;
}

void VelkInstance::dispatch(LogLevel level, const char* file, int line, const char* message)
{
    if (level < level_) {
        return;
    }
    if (sink_) {
        // User-defined sink
        sink_->write(level, file, line, message);
        return;
    }
    // No sink defined, currently writes to stderr regardless of level.
    static const char* level_names[] = {"DEBUG", "INFO", "WARN", "ERROR"};
    auto idx = static_cast<int>(level);
    if (idx < 0 || idx > 3) {
        idx = 3;
    }
    fprintf(stderr, "[%s] %s:%d: %s\n", level_names[idx], file, line, message);
}

} // namespace velk
