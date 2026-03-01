#ifndef VELK_ANIMATOR_INTF_ANIMATOR_PLUGIN_H
#define VELK_ANIMATOR_INTF_ANIMATOR_PLUGIN_H

#include <velk/plugins/animator/interface/intf_animator.h>
#include <velk/interface/intf_plugin.h>

namespace velk {

/** @brief Extended plugin interface exposing the default animator instance. */
class IAnimatorPlugin : public Interface<IAnimatorPlugin>
{
public:
    /** @brief Returns the plugin's default animator. */
    virtual IAnimator& get_default_animator() const = 0;
};

} // namespace velk

#endif // VELK_ANIMATOR_INTF_ANIMATOR_PLUGIN_H
