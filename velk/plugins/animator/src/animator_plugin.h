#ifndef VELK_ANIMATOR_PLUGIN_H
#define VELK_ANIMATOR_PLUGIN_H

#include "animation.h"
#include "animator.h"

#include <velk/ext/plugin.h>
#include <velk/plugins/animator/interface/intf_animator_plugin.h>

namespace velk {

class AnimatorPlugin final : public ext::Plugin<AnimatorPlugin, IAnimatorPlugin>
{
public:
    VELK_PLUGIN_UID("738617b8-dba8-4a08-a0bc-51e8dc4d5faf");
    VELK_PLUGIN_NAME("animator");
    VELK_PLUGIN_VERSION(0, 1, 0);

    ReturnValue initialize(IVelk& velk, PluginConfig& config) override
    {
        config.enableUpdate = true;
        auto rv = register_type<AnimationImpl>(velk);
        if (failed(rv)) {
            return rv;
        }
        rv = register_type<AnimatorImpl>(velk);
        if (failed(rv)) {
            return rv;
        }
        animator_ = velk.create<IAnimator>(ClassId::Animator);
        return ReturnValue::Success;
    }

    ReturnValue shutdown(IVelk&) override { return ReturnValue::Success; }

    void update(const UpdateInfo& info) override
    {
        if (auto* a = interface_cast<IAnimator>(animator_)) {
            a->tick(info);
        }
    }

    IAnimator& get_default_animator() const override { return *animator_; }

private:
    IAnimator::Ptr animator_;
};

} // namespace velk

VELK_PLUGIN(velk::AnimatorPlugin)

#endif // VELK_ANIMATOR_PLUGIN_H
