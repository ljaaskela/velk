#ifndef VELK_ANIMATOR_TRANSITION_IMPL_H
#define VELK_ANIMATOR_TRANSITION_IMPL_H

#include <velk/ext/object.h>
#include <velk/plugins/animator/interface/intf_animated_any.h>
#include <velk/plugins/animator/interface/intf_transition.h>
#include <velk/plugins/animator/plugin.h>

namespace velk {

/**
 * @brief First-class transition object that manages an AnimatedAny for a property.
 *
 * Registered with ClassId::Transition. Wraps AnimatedAny install/uninstall and
 * exposes duration/animating as observable properties.
 */
class TransitionImpl : public ext::Object<TransitionImpl, ITransition>
{
public:
    VELK_CLASS_UID(ClassId::Transition);
    ~TransitionImpl();

    bool tick(Duration dt) override;
    void set_target(const IProperty::Ptr& target) override;
    IProperty::Ptr get_target() const override;
    void set_easing(easing::EasingFn easing) override;

private:
    void install();
    void uninstall();
    ITransition::State* state();
    void notify_state();

    IProperty::Ptr target_;
    IAnimatedAny::Ptr animated_;
    easing::EasingFn easing_ = easing::linear;
    bool installed_ = false;
};

} // namespace velk

#endif // VELK_ANIMATOR_TRANSITION_IMPL_H
