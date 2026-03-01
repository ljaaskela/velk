#ifndef VELK_INTF_PLUGIN_REGISTRY_H
#define VELK_INTF_PLUGIN_REGISTRY_H

#include <velk/interface/intf_plugin.h>

namespace velk {

/**
 * @brief Interface for managing loaded plugins.
 *
 * Plugins are loaded via load_plugin() which calls their initialize() method,
 * and unloaded via unload_plugin() which calls shutdown() and sweeps any
 * types the plugin registered but did not explicitly unregister.
 */
class IPluginRegistry : public Interface<IPluginRegistry>
{
public:
    /** @brief Loads a plugin, calling its initialize() method. */
    virtual ReturnValue load_plugin(const IPlugin::Ptr& plugin) = 0;
    /** @brief Creates and loads a plugin by its registered class UID. */
    virtual ReturnValue load_plugin(Uid pluginUid) = 0;
    /** @brief Loads a plugin from a shared library (.dll/.so) at the given path. */
    virtual ReturnValue load_plugin_from_path(const char* path) = 0;
    /** @brief Unloads a plugin by ID, calling shutdown() and sweeping owned types. */
    virtual ReturnValue unload_plugin(Uid pluginId) = 0;
    /**
     * @brief Finds a loaded plugin by its ID, or nullptr if not loaded.
     *
     * Returns a non-owning pointer. Do not store the result beyond the
     * current scope, as the plugin may be unloaded (and its DLL freed)
     * at any time.
     */
    virtual IPlugin* find_plugin(Uid pluginId) const = 0;
    /** @brief Returns the number of currently loaded plugins. */
    virtual size_t plugin_count() const = 0;

    /** @brief Unloads a plugin by its class type. */
    template <class T>
    ReturnValue unload_plugin()
    {
        return unload_plugin(T::class_id());
    }
    /** @brief Finds a loaded plugin by its class type, or nullptr if not loaded. */
    template <class T>
    IPlugin* find_plugin() const
    {
        return find_plugin(T::class_id());
    }
    /**
     * @brief Returns a plugin instance with given plugin id.
     *        Lazily loads the plugin if not already loaded.
     * @param pluginId Id if the plugin.
     * @return Plugin instance or nullptr if loading failed.
     */
    IPlugin* get_or_load_plugin(Uid pluginId)
    {
        auto* plugin = find_plugin(pluginId);
        if (!plugin) {
            load_plugin(pluginId);
            plugin = find_plugin(pluginId);
        }
        return plugin;
    }
};

/**
 * @brief Typed wrapper for get_or_load_plugin from instance().plugin_registry().
 * @see IPluginRegistry::get_or_load_plugin
 */
template <class T>
T* get_or_load_plugin(Uid pluginId)
{
    auto& reg = instance().plugin_registry();
    return interface_cast<T>(reg.get_or_load_plugin(pluginId));
}

} // namespace velk

#endif // VELK_INTF_PLUGIN_REGISTRY_H
