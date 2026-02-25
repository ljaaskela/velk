#ifndef VELK_PLUGINS_HIVE_PLUGIN_H
#define VELK_PLUGINS_HIVE_PLUGIN_H

#include <velk/ext/plugin.h>
#include <velk/interface/hive/intf_hive_registry.h>

namespace velk {

/**
 * @brief Built-in plugin that registers hive types (HiveRegistry, Hive).
 *
 * After loading this plugin, applications can create HiveRegistry instances
 * via velk.create(ClassId::HiveRegistry).
 */
class HivePlugin : public ext::Plugin<HivePlugin>
{
public:
    VELK_CLASS_UID(ClassId::HivePlugin);
    VELK_PLUGIN_NAME("HivePlugin");
    VELK_PLUGIN_VERSION(0, 1, 0);

    ReturnValue initialize(IVelk& velk, PluginConfig& config) override;
    ReturnValue shutdown(IVelk& velk) override;
};

} // namespace velk

#endif // VELK_PLUGINS_HIVE_PLUGIN_H
