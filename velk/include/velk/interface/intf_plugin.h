#ifndef VELK_INTF_PLUGIN_H
#define VELK_INTF_PLUGIN_H

#include <velk/array_view.h>
#include <velk/interface/intf_object_factory.h>
#include <velk/interface/types.h>

namespace velk {

class IVelk; // Forward declaration

/** @brief Packs major.minor.patch into a single uint32_t.
 *  Layout: [major:8][minor:8][patch:16] */
constexpr uint32_t make_version(uint8_t major, uint8_t minor, uint16_t patch = 0)
{
    return (static_cast<uint32_t>(major) << 24) | (static_cast<uint32_t>(minor) << 16) |
           static_cast<uint32_t>(patch);
}

constexpr uint8_t version_major(uint32_t v)
{
    return static_cast<uint8_t>(v >> 24);
}
constexpr uint8_t version_minor(uint32_t v)
{
    return static_cast<uint8_t>(v >> 16);
}
constexpr uint16_t version_patch(uint32_t v)
{
    return static_cast<uint16_t>(v);
}

/** @brief A dependency on another plugin, optionally with a minimum version. */
struct PluginDependency
{
    Uid uid;                  ///< Uid of the dependency plugin
    uint32_t min_version = 0; ///< 0 = any version

    constexpr PluginDependency(Uid id, uint32_t ver = 0) : uid(id), min_version(ver) {}
};

/**
 * @brief Static descriptor for a plugin: factory, display name, version, and dependencies.
 *
 * The factory provides class identity (UID, class name via ClassInfo) and
 * instance creation. The display name is a human-readable label that may
 * differ from the C++ class name.
 */
struct PluginInfo
{
    const IObjectFactory& factory;             ///< Factory for creating plugin instances.
    string_view name;                          ///< Human-readable display name.
    uint32_t version = 0;                      ///< Plugin version (use make_version).
    array_view<PluginDependency> dependencies; ///< Plugins that must be loaded first.

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
    /** @brief Returns the plugin version. */
    uint32_t get_version() const { return get_plugin_info().version; }
    /** @brief Returns the dependencies of this plugin. */
    array_view<PluginDependency> get_dependencies() const { return get_plugin_info().dependencies; }
};

} // namespace velk

#endif // VELK_INTF_PLUGIN_H
