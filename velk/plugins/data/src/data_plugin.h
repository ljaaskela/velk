#ifndef VELK_DATA_PLUGIN_IMPL_H
#define VELK_DATA_PLUGIN_IMPL_H

#include <velk/ext/plugin.h>
#include <velk/plugins/data/plugin.h>

namespace velk {

class DataPlugin final : public ext::Plugin<DataPlugin>
{
public:
    VELK_PLUGIN_UID("5c7a9e13-2b4d-4f68-8e1a-3d6b0f9c2a57");
    VELK_PLUGIN_NAME("data");
    VELK_PLUGIN_VERSION(0, 1, 0);

    ReturnValue initialize(IVelk& velk, PluginConfig& config) override;
    ReturnValue shutdown(IVelk&) override { return ReturnValue::Success; }
};

} // namespace velk

VELK_PLUGIN(velk::DataPlugin)

#endif // VELK_DATA_PLUGIN_IMPL_H
