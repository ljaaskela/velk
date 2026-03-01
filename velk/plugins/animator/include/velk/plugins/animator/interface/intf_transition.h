#ifndef VELK_ANIMATOR_INTF_TRANSITION_H
#define VELK_ANIMATOR_INTF_TRANSITION_H

#include <velk/interface/intf_metadata.h>
#include <velk/interface/intf_property.h>
#include <velk/plugins/animator/easing.h>

namespace velk {

/**
 * @brief Interface for an implicit property transition.
 *
 * Properties (via VELK_INTERFACE):
 *   - duration  (Duration): Transition duration (read/write).
 *   - animating (bool):     Whether the transition is currently animating (read-only).
 */
class ITransition : public Interface<ITransition>
{
public:
    VELK_INTERFACE(
        (PROP, Duration, duration, {}),
        (PROP, bool,     animating, false)
    )

    /** @brief Advances the transition. Returns true if still animating. */
    virtual bool tick(Duration dt) = 0;
    /** @brief Sets the target property for this transition. */
    virtual void set_target(const IProperty::Ptr& target) = 0;
    /** @brief Returns the target property. */
    virtual IProperty::Ptr get_target() const = 0;
    /** @brief Sets the easing function. */
    virtual void set_easing(easing::EasingFn easing) = 0;
};

} // namespace velk

#endif // VELK_ANIMATOR_INTF_TRANSITION_H
