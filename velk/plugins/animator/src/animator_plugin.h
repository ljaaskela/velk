#ifndef VELK_ANIMATOR_PLUGIN_H
#define VELK_ANIMATOR_PLUGIN_H

#include "animation.h"
#include "animator.h"

#include <velk/ext/plugin.h>
#include <velk/plugins/animator/interpolator_traits.h>
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
        auto& types = velk.type_registry();
        types.register_interpolator<float>(&detail::typed_interpolator<float>);
        types.register_interpolator<double>(&detail::typed_interpolator<double>);
        types.register_interpolator<uint8_t>(&detail::typed_interpolator<uint8_t>);
        types.register_interpolator<uint16_t>(&detail::typed_interpolator<uint16_t>);
        types.register_interpolator<uint32_t>(&detail::typed_interpolator<uint32_t>);
        types.register_interpolator<uint64_t>(&detail::typed_interpolator<uint64_t>);
        types.register_interpolator<int8_t>(&detail::typed_interpolator<int8_t>);
        types.register_interpolator<int16_t>(&detail::typed_interpolator<int16_t>);
        types.register_interpolator<int32_t>(&detail::typed_interpolator<int32_t>);
        types.register_interpolator<int64_t>(&detail::typed_interpolator<int64_t>);

        animator_ = velk.create<IAnimator>(ClassId::Animator);
        return ReturnValue::Success;
    }

    ReturnValue shutdown(IVelk&) override { return ReturnValue::Success; }

    void pre_update(const IPlugin::PreUpdateInfo& info) override
    {
        if (auto* a = interface_cast<IAnimator>(animator_)) {
            a->tick(info.info);
        }
    }

    IAnimator& get_default_animator() const override { return *animator_; }

private:
    IAnimator::Ptr animator_;
};

} // namespace velk

VELK_PLUGIN(velk::AnimatorPlugin)

#endif // VELK_ANIMATOR_PLUGIN_H
