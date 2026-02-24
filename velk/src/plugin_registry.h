#ifndef VELK_PLUGIN_REGISTRY_H
#define VELK_PLUGIN_REGISTRY_H

#include "library_handle.h"
#include "type_registry.h"

#include <velk/common.h>
#include <velk/ext/interface_dispatch.h>
#include <velk/interface/intf_plugin_registry.h>
#include <velk/interface/intf_velk.h>

#include <vector>

namespace velk {

/**
 * @brief Concrete implementation of IPluginRegistry.
 *
 * Maintains a sorted vector of loaded plugins and handles loading,
 * initialization, dependency checking, and shutdown.
 * Owned as a stack member by VelkInstance.
 */
class PluginRegistry final : public ext::InterfaceDispatch<IPluginRegistry>
{
public:
    PluginRegistry(IVelk& velk, TypeRegistry& types);

    // IPluginRegistry overrides
    ReturnValue load_plugin(const IPlugin::Ptr& plugin) override;
    ReturnValue load_plugin_from_path(const char* path) override;
    ReturnValue unload_plugin(Uid pluginId) override;
    IPlugin* find_plugin(Uid pluginId) const override;
    size_t plugin_count() const override;

    /** @brief Shuts down all plugins in reverse order. Called from ~VelkInstance. */
    void shutdown_all();
    /** @brief Notifies opted-in plugins with timing information. */
    void notify_plugins(Duration time) const;

private:
    /** @brief Plugin registry entry. */
    struct PluginEntry
    {
        Uid uid;
        IPlugin::Ptr plugin;
        LibraryHandle library; ///< Non-empty when plugin was loaded from a shared library.
        PluginConfig config;   ///< Configuration set by the plugin during initialize.
        bool operator<(const PluginEntry& o) const { return uid < o.uid; }
    };

    /** @brief Checks that all dependencies declared in info are loaded. Logs and returns Fail if not. */
    ReturnValue check_dependencies(const PluginInfo& info);

    std::vector<PluginEntry> plugins_;        ///< Sorted registry of loaded plugins.
    std::vector<IPlugin*> update_plugins_;    ///< Plugins that opted into update notifications.
    mutable UpdateInfo update_timestamps_;    ///< Absolute timestamps for init, first update, last update.
    mutable bool last_update_was_explicit_{}; ///< Whether previous update used explicit time.
    ILog& log_;
    TypeRegistry& types_;
    IVelk& velk_;
};

} // namespace velk

#endif // VELK_PLUGIN_REGISTRY_H
