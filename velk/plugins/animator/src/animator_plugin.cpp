#include "animated_any.h"
#include "animator_plugin.h"

#include <velk/ext/any.h>

namespace velk {

ReturnValue AnimatorPlugin::initialize(IVelk& velk, PluginConfig& config)
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
    rv = register_type<AnimatedAny>(velk);
    if (failed(rv)) {
        return rv;
    }
    rv = register_type<TransitionImpl>(velk);
    if (failed(rv)) {
        return rv;
    }
    auto& types = velk.type_registry();
    types.register_type<ext::AnyValue<KeyframeEntry>>();
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
    velk_ = &velk;
    return ReturnValue::Success;
}

ReturnValue AnimatorPlugin::shutdown(IVelk&)
{
    return ReturnValue::Success;
}

void AnimatorPlugin::pre_update(const IPlugin::PreUpdateInfo& info)
{
    if (auto* a = interface_cast<IAnimator>(animator_)) {
        a->tick(info.info);
    }
    tick_transitions(info.info);
}

void AnimatorPlugin::register_transition(const ITransition::WeakPtr& transition)
{
    transitions_.push_back(transition);
}

void AnimatorPlugin::tick_transitions(const UpdateInfo& info)
{
    size_t write = 0;
    for (size_t i = 0; i < transitions_.size(); ++i) {
        auto tr = transitions_[i].lock();
        if (!tr) {
            continue;
        }
        tr->tick(info.dt);
        transitions_[write++] = transitions_[i];
    }
    transitions_.resize(write);
}

} // namespace velk
