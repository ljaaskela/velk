#include <velk/ext/object.h>
#include <velk/ext/plugin.h>
#include <velk/interface/intf_velk.h>

using namespace velk;

class IDllWidget : public Interface<IDllWidget>
{
public:
    VELK_INTERFACE(
        (PROP, int, value, 0)
    )
};

class DllWidget : public ext::Object<DllWidget, IDllWidget>
{};

// A sub-plugin that lives in the same DLL but is loaded by the host plugin
class DllSubPlugin : public ext::Plugin<DllSubPlugin>
{
public:
    VELK_PLUGIN_UID("b0000000-0000-0000-0000-000000000002");
    VELK_PLUGIN_NAME("DllSubPlugin");

    ReturnValue initialize(IVelk& velk) override { return register_type<DllWidget>(velk); }

    ReturnValue shutdown(IVelk&) override { return ReturnValue::Success; }
};

// Host plugin: exports the entry point and loads DllSubPlugin during initialize
class DllTestPlugin : public ext::Plugin<DllTestPlugin>
{
public:
    VELK_PLUGIN_UID("b0000000-0000-0000-0000-000000000001"); // optional
    VELK_PLUGIN_NAME("DllTestPlugin");                       // optional

    ReturnValue initialize(IVelk& velk) override
    {
        // Register DllWidget type
        auto rv = register_type<DllWidget>(velk);
        // Load another plugin (DllSubPlugin) manually as part of our initialization process
        return failed(rv) ? rv
                          : velk.plugin_registry().load_plugin(ext::make_object<DllSubPlugin, IPlugin>());
    }

    ReturnValue shutdown(IVelk& velk) override
    {
        velk.plugin_registry().unload_plugin(DllSubPlugin::class_id());
        return ReturnValue::Success;
    }
};

VELK_PLUGIN(DllTestPlugin)
