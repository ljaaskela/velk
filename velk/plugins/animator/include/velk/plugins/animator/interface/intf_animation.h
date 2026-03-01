#ifndef VELK_ANIMATOR_INTF_ANIMATION_H
#define VELK_ANIMATOR_INTF_ANIMATION_H

#include <velk/array_view.h>
#include <velk/interface/intf_metadata.h>
#include <velk/interface/intf_velk.h>
#include <velk/plugins/animator/easing.h>

namespace velk {

/** @brief A type-erased keyframe: time, value, and easing function. */
struct KeyframeEntry
{
    Duration time;
    IAny::Ptr value;
    easing::EasingFn easing = easing::linear;
};

/**
 * @brief Interface for a running animation.
 *
 * Properties (via VELK_INTERFACE):
 *   - duration (Duration): Total duration.
 *   - elapsed  (Duration): Elapsed time.
 *   - finished (bool):     Whether the animation has completed.
 */
class IAnimation : public Interface<IAnimation>
{
public:
    VELK_INTERFACE(
        (PROP, Duration, duration, {}),
        (PROP, Duration, elapsed,  {}),
        (PROP, bool,     finished, false)
    )

    /** @brief Advances the animation. Returns true when finished. */
    virtual bool tick(const UpdateInfo& info) = 0;
    /** @brief Cancels the animation immediately. */
    virtual void cancel() = 0;
    /** @brief Sets the target property to animate. */
    virtual void set_target(const IProperty::Ptr& target) = 0;
    /** @brief Sets an explicit interpolator for this animation, overriding registry lookup. */
    virtual void set_interpolator(InterpolatorFn fn) = 0;
    /** @brief Replaces all keyframes. Shares ownership of each entry's value. */
    virtual void set_keyframes(array_view<KeyframeEntry> keyframes) = 0;
};

} // namespace velk

#endif // VELK_ANIMATOR_INTF_ANIMATION_H
