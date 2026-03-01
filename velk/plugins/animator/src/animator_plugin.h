#ifndef VELK_ANIMATOR_PLUGIN_H
#define VELK_ANIMATOR_PLUGIN_H

#include "animation.h"
#include "animator.h"

#include <velk/ext/plugin.h>
#include <velk/plugins/animator/interpolator_traits.h>
#include <velk/plugins/animator/interface/intf_animated_any.h>
#include <velk/plugins/animator/interface/intf_animator_plugin.h>

namespace velk {

class AnimatorPlugin final : public ext::Plugin<AnimatorPlugin, IAnimatorPlugin>
{
public:
    VELK_PLUGIN_UID("738617b8-dba8-4a08-a0bc-51e8dc4d5faf");
    VELK_PLUGIN_NAME("animator");
    VELK_PLUGIN_VERSION(0, 1, 0);

    ReturnValue initialize(IVelk& velk, PluginConfig& config) override;
    ReturnValue shutdown(IVelk&) override;
    void pre_update(const IPlugin::PreUpdateInfo& info) override;

    IAnimator& get_default_animator() const override { return *animator_; }

    void set_transition(const IProperty::Ptr& prop, Duration duration,
                        easing::EasingFn easing) override;
    void clear_transition(const IProperty::Ptr& prop) override;

private:
    struct TransitionEntry
    {
        IProperty::Ptr prop;
        IAnimatedAny::Ptr animated;
    };

    void tick_animated_anys(const UpdateInfo& info);

    IAnimator::Ptr animator_;
    IVelk* velk_ = nullptr;
    vector<TransitionEntry> transitions_;
};

} // namespace velk

VELK_PLUGIN(velk::AnimatorPlugin)

#endif // VELK_ANIMATOR_PLUGIN_H
