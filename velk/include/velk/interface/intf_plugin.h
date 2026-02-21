#ifndef VELK_INTF_PLUGIN_H
#define VELK_INTF_PLUGIN_H

#include <velk/array_view.h>
#include <velk/interface/intf_object_factory.h>
#include <velk/interface/types.h>

namespace velk {

class IVelk; // Forward declaration

/**
 * @brief Static descriptor for a plugin: factory, display name, and dependencies.
 *
 * The factory provides class identity (UID, class name via ClassInfo) and
 * instance creation. The display name is a human-readable label that may
 * differ from the C++ class name.
 */
struct PluginInfo {
    const IObjectFactory& factory;  ///< Factory for creating plugin instances.
    string_view name;               ///< Human-readable display name.
    array_view<Uid> dependencies;   ///< UIDs of plugins that must be loaded first.

    /** @brief Returns the plugin UID from the factory's ClassInfo. */
    Uid uid() const { return factory.get_class_info().uid; }
};

/**
 * @brief Interface that plugins implement to register types and hook into the Velk runtime.
 *
 * Inherits IObject so that plugin identity (get_class_uid, get_class_name)
 * and lifetime (shared_ptr via get_self) are available directly from an IPlugin reference.
 *
 * Plugin authors derive via ext::Plugin<T> and implement initialize, shutdown,
 * and optionally declare static metadata (plugin_name, plugin_deps).
 */
class IPlugin : public Interface<IPlugin, IObject>
{
public:
    /** @brief Called when the plugin is loaded. Register types and perform setup here. */
    virtual ReturnValue initialize(IVelk& velk) = 0;
    /** @brief Called when the plugin is unloaded. Unregister types and clean up here. */
    virtual ReturnValue shutdown(IVelk& velk) = 0;
    /** @brief Returns the static plugin descriptor. */
    virtual const PluginInfo& get_plugin_info() const = 0;

    /** @brief Returns the human-readable name of this plugin. */
    string_view get_name() const { return get_plugin_info().name; }
    /** @brief Returns the UIDs of plugins that must be loaded before this one. */
    array_view<Uid> get_dependencies() const { return get_plugin_info().dependencies; }
};

} // namespace velk

#endif // VELK_INTF_PLUGIN_H
