#include "velk_instance.h"

#include "function.h"
#include "future.h"
#include "metadata_container.h"
#include "property.h"

#include <velk/ext/any.h>
#include <velk/ext/plugin.h>
#include <velk/interface/types.h>

#include <algorithm>
#include <chrono>

namespace velk {

static int64_t now_us()
{
    using namespace std::chrono;
    return duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
}

void RegisterTypes(ITypeRegistry& reg)
{
    reg.register_type<PropertyImpl>();
    reg.register_type<FunctionImpl>();
    reg.register_type<FutureImpl>();

    reg.register_type<ext::AnyValue<float>>();
    reg.register_type<ext::AnyValue<double>>();
    reg.register_type<ext::AnyValue<uint8_t>>();
    reg.register_type<ext::AnyValue<uint16_t>>();
    reg.register_type<ext::AnyValue<uint32_t>>();
    reg.register_type<ext::AnyValue<uint64_t>>();
    reg.register_type<ext::AnyValue<int8_t>>();
    reg.register_type<ext::AnyValue<int16_t>>();
    reg.register_type<ext::AnyValue<int32_t>>();
    reg.register_type<ext::AnyValue<int64_t>>();
    reg.register_type<ext::AnyValue<std::string>>();
}

VelkInstance::VelkInstance() : init_time_us_(now_us())
{
    RegisterTypes(*this);
}

VelkInstance::~VelkInstance()
{
    update_plugins_.clear();

    // Unload plugins in reverse order so that dependents shut down before
    // their dependencies.
    while (!plugins_.empty()) {
        auto& entry = plugins_.back();
        entry.plugin->shutdown(*this);

        // Sweep types owned by this plugin unless it opted to retain them.
        if (!entry.config.retainTypesOnUnload) {
            Uid owner = entry.uid;
            types_.erase(std::remove_if(
                             types_.begin(), types_.end(), [&](const Entry& e) { return e.owner == owner; }),
                         types_.end());
        }

        // Move library handle out before erasing so it outlives the plugin pointer.
        auto handle = std::move(entry.library);
        plugins_.pop_back();
        handle.close();
    }
}

const IObjectFactory* VelkInstance::find(Uid uid) const
{
    Entry key{uid, nullptr};
    auto it = std::lower_bound(types_.begin(), types_.end(), key);
    if (it != types_.end() && it->uid == uid) {
        return it->factory;
    }
    return nullptr;
}

ILog& get_logger(const VelkInstance& instance)
{
    /** @note VelkInstance should generally log through direct call to
     *  detail::velk_log(get_logger(*this), ...) because VelkInstance may
     *  still be in the process of being constructed, hence making the global
     *  ::velk::instance() unavailable
     */
    return static_cast<ILog&>(*const_cast<VelkInstance*>(&instance));
}

ReturnValue VelkInstance::register_type(const IObjectFactory& factory)
{
    auto& info = factory.get_class_info();
    detail::velk_log(get_logger(*this),
                     LogLevel::Debug,
                     __FILE__,
                     __LINE__,
                     "Register %.*s",
                     static_cast<int>(info.name.size()),
                     info.name.data());
    Entry entry{info.uid, &factory, current_owner_};
    auto it = std::lower_bound(types_.begin(), types_.end(), entry);
    if (it != types_.end() && it->uid == info.uid) {
        it->factory = &factory;
        it->owner = current_owner_;
    } else {
        types_.insert(it, entry);
    }
    return ReturnValue::Success;
}

ReturnValue VelkInstance::unregister_type(const IObjectFactory& factory)
{
    Entry key{factory.get_class_info().uid, nullptr};
    auto it = std::lower_bound(types_.begin(), types_.end(), key);
    if (it != types_.end() && it->uid == key.uid) {
        types_.erase(it);
    }
    return ReturnValue::Success;
}

IInterface::Ptr VelkInstance::create(Uid uid) const
{
    if (auto* factory = find(uid)) {
        if (auto object = factory->create_instance()) {
            if (auto* meta = interface_cast<IMetadataContainer>(object)) {
                // Object can contain metadata
                auto& info = factory->get_class_info();
                // Object takes ownership of the MetadataContainer
                meta->set_metadata_container(new MetadataContainer(info.members, object.get()));
            }
            return object;
        }
    }
    return {};
}

const ClassInfo* VelkInstance::get_class_info(Uid classUid) const
{
    if (auto* factory = find(classUid)) {
        return &factory->get_class_info();
    }
    return nullptr;
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

void VelkInstance::update(Duration time) const
{
    // Swap the queue under lock, then invoke outside the lock.
    // Tasks queued during invocation (by deferred handlers) will be picked up at the next update().
    std::vector<DeferredTask> tasks;
    {
        std::lock_guard lock(deferred_mutex_);
        tasks.swap(deferred_queue_);
    }
    for (auto& task : tasks) {
        if (task.fn) {
            task.fn->invoke(task.args ? task.args->view() : FnArgs{});
        }
    }

    // Notify plugins that opted into update notifications.
    if (!update_plugins_.empty()) {
        bool is_explicit = time.us != 0;
        int64_t current_us = is_explicit ? time.us : now_us();

        // Reset tracking when switching between explicit and auto time domains.
        if (is_explicit != last_update_was_explicit_) {
            first_update_us_ = 0;
            last_update_us_ = 0;
        }
        last_update_was_explicit_ = is_explicit;

        if (!first_update_us_) {
            first_update_us_ = current_us;
        }

        UpdateInfo info;
        info.timeSinceInit = {current_us - init_time_us_};
        info.timeSinceFirstUpdate = {current_us - first_update_us_};
        info.timeSinceLastUpdate = {last_update_us_ ? current_us - last_update_us_ : 0};
        last_update_us_ = current_us;

        for (auto* plugin : update_plugins_) {
            plugin->update(info);
        }
    }
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

ReturnValue VelkInstance::check_dependencies(const PluginInfo& info)
{
    for (auto& dep : info.dependencies) {
        auto* plugin = find_plugin(dep.uid);
        if (!plugin) {
            detail::velk_log(get_logger(*this),
                             LogLevel::Error,
                             __FILE__,
                             __LINE__,
                             "Plugin '%.*s' has unmet dependency: %016llx%016llx",
                             static_cast<int>(info.name.size()),
                             info.name.data(),
                             static_cast<unsigned long long>(dep.uid.hi),
                             static_cast<unsigned long long>(dep.uid.lo));
            return ReturnValue::Fail;
        }
        if (dep.min_version && plugin->get_version() < dep.min_version) {
            detail::velk_log(get_logger(*this),
                             LogLevel::Error,
                             __FILE__,
                             __LINE__,
                             "Plugin '%.*s' requires version %u.%u.%u, got %u.%u.%u",
                             static_cast<int>(info.name.size()),
                             info.name.data(),
                             version_major(dep.min_version),
                             version_minor(dep.min_version),
                             version_patch(dep.min_version),
                             version_major(plugin->get_version()),
                             version_minor(plugin->get_version()),
                             version_patch(plugin->get_version()));
            return ReturnValue::Fail;
        }
    }
    return ReturnValue::Success;
}

ReturnValue VelkInstance::load_plugin_from_path(const char* path)
{
    if (!path || !*path) {
        return ReturnValue::InvalidArgument;
    }

    auto lib = LibraryHandle::open(path);
    if (!lib) {
        detail::velk_log(
            get_logger(*this), LogLevel::Error, __FILE__, __LINE__, "Failed to load library: %s", path);
        return ReturnValue::Fail;
    }

    auto* get_info = reinterpret_cast<detail::PluginInfoFn*>(lib.symbol("velk_plugin_info"));
    if (!get_info) {
        detail::velk_log(get_logger(*this),
                         LogLevel::Error,
                         __FILE__,
                         __LINE__,
                         "Library missing velk_plugin_info entry point: %s",
                         path);
        lib.close();
        return ReturnValue::Fail;
    }

    auto& info = *get_info();

    // Check for duplicates and dependencies before instantiating.
    Uid id = info.uid();
    PluginEntry key{id, {}};
    auto it = std::lower_bound(plugins_.begin(), plugins_.end(), key);
    if (it != plugins_.end() && it->uid == id) {
        lib.close();
        return ReturnValue::NothingToDo;
    }
    if (auto rv = check_dependencies(info); failed(rv)) {
        lib.close();
        return rv;
    }

    // Create the plugin instance via the factory (properly sets up control block).
    auto plugin = info.factory.create_instance<IPlugin>();
    if (!plugin) {
        detail::velk_log(get_logger(*this),
                         LogLevel::Error,
                         __FILE__,
                         __LINE__,
                         "Factory failed to create plugin: %s",
                         path);
        lib.close();
        return ReturnValue::Fail;
    }

    ReturnValue rv = load_plugin(plugin);
    if (succeeded(rv)) {
        it = std::lower_bound(plugins_.begin(), plugins_.end(), key);
        it->library = std::move(lib);
    } else {
        lib.close();
    }
    return rv;
}

ReturnValue VelkInstance::load_plugin(const IPlugin::Ptr& plugin)
{
    if (!plugin) {
        return ReturnValue::InvalidArgument;
    }
    Uid id = plugin->get_class_uid();
    PluginEntry key{id, {}};
    auto it = std::lower_bound(plugins_.begin(), plugins_.end(), key);
    if (it != plugins_.end() && it->uid == id) {
        return ReturnValue::NothingToDo; // Already there
    }
    if (auto rv = check_dependencies(plugin->get_plugin_info()); failed(rv)) {
        return rv;
    }
    it = plugins_.insert(it, PluginEntry{id, plugin});

    PluginConfig config;
    current_owner_ = id;
    ReturnValue rv = plugin->initialize(*this, config);
    current_owner_ = Uid{};

    it->config = config;

    if (failed(rv)) {
        plugins_.erase(it);
        return rv;
    }

    if (config.enableUpdate) {
        update_plugins_.push_back(plugin.get());
    }
    return ReturnValue::Success;
}

ReturnValue VelkInstance::unload_plugin(Uid pluginId)
{
    PluginEntry key{pluginId, {}};
    auto it = std::lower_bound(plugins_.begin(), plugins_.end(), key);
    if (it == plugins_.end() || it->uid != pluginId) {
        return ReturnValue::InvalidArgument;
    }

    // Reject if any other loaded plugin depends on this one.
    for (auto& pe : plugins_) {
        if (pe.uid == pluginId) {
            continue;
        }
        for (auto& dep : pe.plugin->get_dependencies()) {
            if (dep.uid == pluginId) {
                detail::velk_log(get_logger(*this),
                                 LogLevel::Error,
                                 __FILE__,
                                 __LINE__,
                                 "Cannot unload plugin '%.*s': plugin '%.*s' depends on it",
                                 static_cast<int>(it->plugin->get_name().size()),
                                 it->plugin->get_name().data(),
                                 static_cast<int>(pe.plugin->get_name().size()),
                                 pe.plugin->get_name().data());
                return ReturnValue::Fail;
            }
        }
    }

    IPlugin* raw = it->plugin.get();
    it->plugin->shutdown(*this);

    // Remove from update cache.
    auto uit = std::find(update_plugins_.begin(), update_plugins_.end(), raw);
    if (uit != update_plugins_.end()) {
        update_plugins_.erase(uit);
    }

    // Sweep types owned by this plugin unless it opted to retain them.
    if (!it->config.retainTypesOnUnload) {
        types_.erase(
            std::remove_if(types_.begin(), types_.end(), [&](const Entry& e) { return e.owner == pluginId; }),
            types_.end());
    }

    // Move library handle out before erasing so it can be freed after the plugin pointer is gone.
    auto handle = std::move(it->library);
    plugins_.erase(it);
    handle.close();
    return ReturnValue::Success;
}

IPlugin* VelkInstance::find_plugin(Uid pluginId) const
{
    PluginEntry key{pluginId, {}};
    auto it = std::lower_bound(plugins_.begin(), plugins_.end(), key);
    if (it != plugins_.end() && it->uid == pluginId) {
        return it->plugin.get();
    }
    return nullptr;
}

size_t VelkInstance::plugin_count() const
{
    return plugins_.size();
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
