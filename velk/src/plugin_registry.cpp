#include "plugin_registry.h"

#include <velk/ext/plugin.h>
#include <velk/interface/intf_log.h>

#include <algorithm>
#include <chrono>

namespace velk {

static int64_t now_us()
{
    using namespace std::chrono;
    return duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
}

PluginRegistry::PluginRegistry(IVelk& velk, TypeRegistry& types)
    : update_timestamps_{{now_us()}, {}, {}},
      log_(velk.log()),
      types_(types),
      velk_(velk)
{}

ReturnValue PluginRegistry::check_dependencies(const PluginInfo& info)
{
    for (auto& dep : info.dependencies) {
        auto* plugin = find_plugin(dep.uid);
        if (!plugin) {
            detail::velk_log(log_,
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
            detail::velk_log(log_,
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

ReturnValue PluginRegistry::load_plugin(const IPlugin::Ptr& plugin)
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
    types_.set_owner(id);
    ReturnValue rv = plugin->initialize(velk_, config);
    types_.set_owner(Uid{});

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

ReturnValue PluginRegistry::load_plugin_from_path(const char* path)
{
    if (!path || !*path) {
        return ReturnValue::InvalidArgument;
    }

    auto lib = LibraryHandle::open(path);
    if (!lib) {
        detail::velk_log(log_, LogLevel::Error, __FILE__, __LINE__, "Failed to load library: %s", path);
        return ReturnValue::Fail;
    }

    auto* get_info = reinterpret_cast<detail::PluginInfoFn*>(lib.symbol("velk_plugin_info"));
    if (!get_info) {
        detail::velk_log(log_,
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
        detail::velk_log(
            log_, LogLevel::Error, __FILE__, __LINE__, "Factory failed to create plugin: %s", path);
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

ReturnValue PluginRegistry::unload_plugin(Uid pluginId)
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
                detail::velk_log(log_,
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
    it->plugin->shutdown(velk_);

    // Remove from update cache.
    auto uit = std::find(update_plugins_.begin(), update_plugins_.end(), raw);
    if (uit != update_plugins_.end()) {
        update_plugins_.erase(uit);
    }

    // Sweep types owned by this plugin unless it opted to retain them.
    if (!it->config.retainTypesOnUnload) {
        types_.sweep_owner(pluginId);
    }

    // Move library handle out before erasing so it can be freed after the plugin pointer is gone.
    auto handle = std::move(it->library);
    plugins_.erase(it);
    handle.close();
    return ReturnValue::Success;
}

IPlugin* PluginRegistry::find_plugin(Uid pluginId) const
{
    PluginEntry key{pluginId, {}};
    auto it = std::lower_bound(plugins_.begin(), plugins_.end(), key);
    if (it != plugins_.end() && it->uid == pluginId) {
        return it->plugin.get();
    }
    return nullptr;
}

size_t PluginRegistry::plugin_count() const
{
    return plugins_.size();
}

void PluginRegistry::shutdown_all()
{
    update_plugins_.clear();

    // Unload plugins in reverse order so that dependents shut down before
    // their dependencies.
    while (!plugins_.empty()) {
        auto& entry = plugins_.back();
        entry.plugin->shutdown(velk_);

        // Sweep types owned by this plugin unless it opted to retain them.
        if (!entry.config.retainTypesOnUnload) {
            types_.sweep_owner(entry.uid);
        }

        // Move library handle out before erasing so it outlives the plugin pointer.
        auto handle = std::move(entry.library);
        plugins_.pop_back();
        handle.close();
    }
}

void PluginRegistry::notify_plugins(Duration time) const
{
    bool is_explicit = time.us != 0;
    int64_t current_us = is_explicit ? time.us : now_us();
    auto& t = update_timestamps_;

    // Reset tracking when switching between explicit and auto time domains.
    if (is_explicit != last_update_was_explicit_) {
        t.timeSinceFirstUpdate = {};
        t.timeSinceLastUpdate = {};
    }
    last_update_was_explicit_ = is_explicit;

    if (!t.timeSinceFirstUpdate.us) {
        t.timeSinceFirstUpdate.us = current_us;
    }

    UpdateInfo info;
    info.timeSinceInit = {current_us - t.timeSinceInit.us};
    info.timeSinceFirstUpdate = {current_us - t.timeSinceFirstUpdate.us};
    info.timeSinceLastUpdate = {t.timeSinceLastUpdate.us ? current_us - t.timeSinceLastUpdate.us : 0};
    t.timeSinceLastUpdate.us = current_us;

    for (auto* plugin : update_plugins_) {
        plugin->update(info);
    }
}

} // namespace velk
