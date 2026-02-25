#include "hive_plugin.h"

#include "hive.h"
#include "hive_registry.h"

namespace velk {

ReturnValue HivePlugin::initialize(IVelk& velk, PluginConfig& /*config*/)
{
    register_type<HiveRegistry>(velk);
    register_type<Hive>(velk);
    return ReturnValue::Success;
}

ReturnValue HivePlugin::shutdown(IVelk& /*velk*/)
{
    return ReturnValue::Success;
}

} // namespace velk
