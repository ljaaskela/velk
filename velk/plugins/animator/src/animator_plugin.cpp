#include "animated_any.h"
#include "animator_plugin.h"

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
    tick_animated_anys(info.info);
}

void AnimatorPlugin::set_transition(const IProperty::Ptr& prop, Duration duration,
                                    easing::EasingFn easing)
{
    if (!prop || !velk_) {
        return;
    }
    auto* pi = interface_cast<IPropertyInternal>(prop);
    if (!pi) {
        return;
    }

    // Check if this property already has a transition
    for (auto& entry : transitions_) {
        if (entry.prop.get() == prop.get()) {
            entry.animated->set_config({duration, easing});
            return;
        }
    }

    // Look up interpolator from the property's compatible type
    InterpolatorFn interpolator = nullptr;
    if (auto existing = pi->get_any()) {
        auto types = existing->get_compatible_types();
        if (types.size() > 0) {
            interpolator = velk_->type_registry().find_interpolator(types[0]);
        }
    }

    // Create AnimatedAny through the type registry (one object, two views)
    auto animated = velk_->create<IAnimatedAny>(ClassId::AnimatedAny);

    // Swap into property, getting the original back
    IAny::Ptr previous;
    pi->set_any(interface_pointer_cast<IAny>(animated), &previous);

    // Initialize with the original any
    TransitionConfig config{duration, easing};
    animated->init(std::move(previous), config, interpolator, prop);

    // Register for ticking
    transitions_.push_back({prop, std::move(animated)});
}

void AnimatorPlugin::clear_transition(const IProperty::Ptr& prop)
{
    if (!prop) {
        return;
    }
    auto* pi = interface_cast<IPropertyInternal>(prop);
    if (!pi) {
        return;
    }

    for (size_t i = 0; i < transitions_.size(); ++i) {
        if (transitions_[i].prop.get() != prop.get()) {
            continue;
        }

        // Take the entry before erasing
        auto animated = std::move(transitions_[i].animated);
        transitions_.erase(&transitions_[i]);

        // Restore the original any
        auto original = animated->take_original();
        if (original) {
            pi->set_any(original);
        }
        return;
    }
}

void AnimatorPlugin::tick_animated_anys(const UpdateInfo& info)
{
    for (auto& entry : transitions_) {
        entry.animated->tick(info.dt);
    }
}

} // namespace velk
