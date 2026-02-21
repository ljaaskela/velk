#ifndef VELK_INTF_PLUGIN_H
#define VELK_INTF_PLUGIN_H

#include <velk/interface/intf_object.h>
#include <velk/interface/types.h>

namespace velk {

class IVelk; // Forward declaration

/**
 * @brief Interface that plugins implement to register types and hook into the Velk runtime.
 *
 * Inherits IObject so that plugin identity (get_class_uid, get_class_name)
 * and lifetime (shared_ptr via get_self) are available directly from an IPlugin reference.
 *
 * Plugin authors derive via ext::Plugin<T> and implement initialize, shutdown, and name.
 */
class IPlugin : public Interface<IPlugin, IObject>
{
public:
    /** @brief Called when the plugin is loaded. Register types and perform setup here. */
    virtual ReturnValue initialize(IVelk& velk) = 0;
    /** @brief Called when the plugin is unloaded. Unregister types and clean up here. */
    virtual ReturnValue shutdown(IVelk& velk) = 0;
    /** @brief Returns the human-readable name of this plugin. */
    virtual string_view get_name() const = 0;
};

} // namespace velk

#endif // VELK_INTF_PLUGIN_H
