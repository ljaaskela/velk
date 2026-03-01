#ifndef VELK_ANIMATOR_INTF_ANIMATOR_PLUGIN_H
#define VELK_ANIMATOR_INTF_ANIMATOR_PLUGIN_H

#include <velk/plugins/animator/interface/intf_animator.h>
#include <velk/plugins/animator/easing.h>
#include <velk/interface/intf_plugin.h>
#include <velk/interface/intf_property.h>

namespace velk {

/** @brief Extended plugin interface exposing the default animator instance. */
class IAnimatorPlugin : public Interface<IAnimatorPlugin>
{
public:
    /** @brief Returns the plugin's default animator. */
    virtual IAnimator& get_default_animator() const = 0;

    /** @brief Installs an implicit transition on a property. */
    virtual void set_transition(const IProperty::Ptr& prop, Duration duration,
                                easing::EasingFn easing) = 0;

    /** @brief Removes an implicit transition from a property. */
    virtual void clear_transition(const IProperty::Ptr& prop) = 0;
};

} // namespace velk

#endif // VELK_ANIMATOR_INTF_ANIMATOR_PLUGIN_H
