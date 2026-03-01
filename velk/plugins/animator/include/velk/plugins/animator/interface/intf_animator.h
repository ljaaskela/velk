#ifndef VELK_ANIMATOR_INTF_ANIMATOR_H
#define VELK_ANIMATOR_INTF_ANIMATOR_H

#include <velk/interface/intf_metadata.h>
#include <velk/plugins/animator/interface/intf_animation.h>

namespace velk {

/**
 * @brief Interface for an animator that manages and ticks a set of animations.
 *
 * Animations are added via add() and advanced each frame via tick().
 * Finished animations are removed automatically.
 */
class IAnimator : public Interface<IAnimator>
{
public:
    /** @brief Advances all managed animations. Removes finished ones. */
    virtual void tick(const UpdateInfo& info) = 0;
    /** @brief Adds an animation object to be managed. */
    virtual void add(const IAnimation::Ptr& animation) = 0;
    /** @brief Cancels all managed animations. */
    virtual void cancel_all() = 0;
    /** @brief Returns the number of currently active animations. */
    virtual size_t active_count() const = 0;
};

} // namespace velk

#endif // VELK_ANIMATOR_INTF_ANIMATOR_H
