#ifndef VELK_ANIMATOR_INTF_ANIMATED_ANY_H
#define VELK_ANIMATOR_INTF_ANIMATED_ANY_H

#include <velk/interface/intf_any_extension.h>
#include <velk/interface/intf_property.h>
#include <velk/interface/intf_type_registry.h>
#include <velk/plugins/animator/transition.h>

namespace velk {

/** @brief Interface for an IAny wrapper that intercepts writes to animate value changes. */
class IAnimatedAny : public Interface<IAnimatedAny, IAnyExtension>
{
public:
    /**
     * @brief Initializes the animated any after install_extension has set the inner.
     * @param config Transition configuration (duration + easing).
     * @param interpolator Interpolator function for this type.
     * @param property The property this animated any is installed on.
     */
    virtual void init(const TransitionConfig& config, InterpolatorFn interpolator,
                      const IProperty::Ptr& property) = 0;

    /** @brief Advances the animation by dt. Returns true if still animating. */
    virtual bool tick(Duration dt) = 0;

    /** @brief Returns true if currently animating. */
    virtual bool is_animating() const = 0;

    /** @brief Updates the transition config. */
    virtual void set_config(const TransitionConfig& config) = 0;
};

} // namespace velk

#endif // VELK_ANIMATOR_INTF_ANIMATED_ANY_H
