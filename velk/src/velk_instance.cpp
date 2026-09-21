#include "velk_instance.h"

#include "hive/raw_hive.h"
#include "object_storage.h"
#include "resource/file_protocol.h"
#include "resource/memory_protocol.h"
#include "manual_task_pool.h"
#include "threaded_task_pool.h"

#include <velk/interface/intf_storage_owned.h>
#include <velk/interface/types.h>

namespace velk {

static IRawHive::Ptr create_metadata_hive()
{
    auto obj = ext::make_object<impl::RawHive>();
    auto* hive = static_cast<impl::RawHive*>(obj.get());
    hive->init(type_uid<ObjectStorage>(), sizeof(ObjectStorage), alignof(ObjectStorage));
    return interface_pointer_cast<IRawHive>(obj);
}

VelkInstance::VelkInstance()
    : metadata_hive_(create_metadata_hive()),
      type_registry_(*this),
      plugin_registry_(*this, type_registry_)
{
    type_registry_.register_type(FileProtocol::get_factory());
    type_registry_.register_type(MemoryProtocol::get_factory());
    type_registry_.register_type(impl::ThreadedTaskPool::get_factory());
    type_registry_.register_type(impl::ManualTaskPool::get_factory());

    // Shared background pool. No worker threads start until the first submit.
    task_pool_ = interface_pointer_cast<ITaskPool>(ext::make_object<impl::ThreadedTaskPool>());

    // Register file:// protocol (absolute paths).
    auto file_proto = ext::make_object<FileProtocol>();
    resource_store_.register_protocol(interface_pointer_cast<IResourceProtocol>(file_proto));

    // Register memory:// protocol. Plugins that receive embedded bytes
    // (e.g. images inside a .glb) register them here and feed the URIs
    // through the existing decoder pipeline.
    auto memory_proto = ext::make_object<MemoryProtocol>();
    resource_store_.register_protocol(interface_pointer_cast<IResourceProtocol>(memory_proto));

    // app:// is platform-specific (cwd on desktop, AAssetManager on Android,
    // etc.) and is registered by the active runtime plugin.
}

VelkInstance::~VelkInstance()
{
    perf_log_.print_stats();
    // Join the workers before plugins unload, so no task runs code from an unloaded library.
    task_pool_ = nullptr;
    plugin_registry_.shutdown_all();
}

ILog& get_logger(const VelkInstance& instance)
{
    return static_cast<ILog&>(*const_cast<VelkInstance*>(&instance));
}

IObjectStorage* VelkInstance::create_metadata_container(const ClassInfo& info, IInterface* owner) const
{
    return metadata_hive_.emplace(info.members, owner);
}

void VelkInstance::destroy_metadata_container(IObjectStorage* storage) const
{
    metadata_hive_.deallocate(static_cast<ObjectStorage*>(storage));
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

void VelkInstance::flush_deferred_properties(vector<DeferredPropertySet>& propSets) const
{
    // Coalesce property sets: walk backwards, lock each weak_ptr once, keep last-write-wins.
    // Entries with null value are notification-only (value already written via set_value_silent).
    struct CoalescedEntry
    {
        IPropertyInternal::Ptr property;
        IAny* value; // null = notification-only (value already applied)
    };
    vector<CoalescedEntry> unique;
    unique.reserve(propSets.size()); // Assume we have mostly unique properties
    for (size_t i = propSets.size(); i > 0; --i) {
        auto& entry = propSets[i - 1];
        auto locked = entry.property.lock();
        if (!locked) {
            continue;
        }
        bool found = false;
        for (auto& u : unique) {
            if (u.property.get() == locked.get()) {
                // If existing entry is notification-only but this one has a value,
                // upgrade to the value entry (last-write-wins for value entries).
                if (!u.value && entry.value) {
                    u.value = entry.value.get();
                }
                found = true;
                break;
            }
        }
        if (!found) {
            unique.emplace_back(CoalescedEntry{std::move(locked), entry.value.get()});
        }
    }
    // First pass: apply all values silently in original queue order, collect those needing notification.
    vector<IPropertyInternal*> notify;
    notify.reserve(unique.size()); // Assume that values mostly change
    for (size_t i = unique.size(); i > 0; --i) {
        auto& u = unique[i - 1];
        if (u.value) {
            // Standard deferred write: apply value and notify if changed.
            if (u.property->set_value_silent(*u.value) == ReturnValue::Success) {
                notify.push_back(u.property.get());
            }
        } else {
            // Notification-only: value was already written, just fire on_changed.
            notify.push_back(u.property.get());
        }
    }
    // Second pass: fire on_changed for all properties that changed.
    for (auto* prop : notify) {
        if (auto* owned = interface_cast<IStorageOwned>(prop)) {
            ::velk::invoke_property_changed(owned->get_owner(), owned->get_storage_id(), prop);
        } else if (prop) {
            invoke_event(prop->on_changed(), prop->get_any().get());
        }
    }
}

VelkStats VelkInstance::get_stats() const
{
    VelkStats stats;
    stats.types = type_registry_.gather_type_stats();
    stats.plugins = plugin_registry_.gather_plugin_stats();
    return stats;
}

void VelkInstance::update(Duration time) const
{
    // Pre-update: let plugins produce work (tasks, deferred property updates).
    auto info = plugin_registry_.pre_update_plugins(time);

    // Swap the queues under lock, then invoke outside the lock.
    // Tasks queued during invocation (by deferred handlers) will be picked up at the next update().
    vector<DeferredTask> tasks;
    vector<DeferredPropertySet> propSets;
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

    // Post-update: Let plugins observe resolved state.
    plugin_registry_.post_update_plugins({info, tasks.size(), propSets.size()});
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
