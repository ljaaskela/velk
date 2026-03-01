#include "transition.h"

#include <velk/api/velk.h>
#include <velk/plugins/animator/interface/intf_animator_plugin.h>

namespace velk {

TransitionImpl::~TransitionImpl()
{
    uninstall();
}

ITransition::State* TransitionImpl::state()
{
    return static_cast<ITransition::State*>(get_property_state(ITransition::UID));
}

void TransitionImpl::notify_state()
{
    notify(MemberKind::Property, ITransition::UID, Notification::Changed);
}

void TransitionImpl::set_target(const IProperty::Ptr& target)
{
    if (installed_) {
        uninstall();
    }
    target_ = target;
    if (target_) {
        install();
        auto* ap = get_or_load_plugin<IAnimatorPlugin>(PluginId::AnimatorPlugin);
        if (ap) {
            ap->register_transition(ITransition::WeakPtr(get_self<ITransition>()));
        }
    }
}

IProperty::Ptr TransitionImpl::get_target() const
{
    return target_;
}

void TransitionImpl::set_easing(easing::EasingFn easing)
{
    easing_ = easing;
    if (animated_) {
        auto* s = state();
        animated_->set_config({s ? s->duration : Duration{}, easing_});
    }
}

void TransitionImpl::install()
{
    if (!target_ || installed_) {
        return;
    }
    auto* pi = interface_cast<IPropertyInternal>(target_);
    if (!pi) {
        return;
    }

    // Resolve interpolator from the property's compatible type
    InterpolatorFn interpolator = nullptr;
    if (auto existing = pi->get_any()) {
        auto types = existing->get_compatible_types();
        if (types.size() > 0) {
            interpolator = instance().type_registry().find_interpolator(types[0]);
        }
    }

    // Create AnimatedAny
    animated_ = instance().create<IAnimatedAny>(ClassId::AnimatedAny);

    // Swap into property, getting the original back
    IAny::Ptr previous;
    pi->set_any(interface_pointer_cast<IAny>(animated_), &previous);

    // Initialize with the original any
    auto* s = state();
    TransitionConfig config{s ? s->duration : Duration{}, easing_};
    animated_->init(std::move(previous), config, interpolator, target_);

    installed_ = true;
}

void TransitionImpl::uninstall()
{
    if (!installed_ || !animated_ || !target_) {
        return;
    }
    auto* pi = interface_cast<IPropertyInternal>(target_);
    if (!pi) {
        return;
    }

    auto original = animated_->take_original();
    if (original) {
        pi->set_any(original);
    }

    animated_ = nullptr;
    installed_ = false;
}

bool TransitionImpl::tick(Duration dt)
{
    if (!installed_ || !animated_) {
        return false;
    }

    // Sync config in case duration was changed via the property
    auto* s = state();
    if (s) {
        animated_->set_config({s->duration, easing_});
    }

    animated_->tick(dt);

    if (s) {
        bool anim = animated_->is_animating();
        if (s->animating != anim) {
            s->animating = anim;
            notify_state();
        }
    }

    return animated_->is_animating();
}

} // namespace velk
