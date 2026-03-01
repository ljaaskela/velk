#ifndef VELK_ANIMATOR_INTF_ANIMATOR_H
#define VELK_ANIMATOR_INTF_ANIMATOR_H

#include <velk/interface/intf_metadata.h>
#include <velk/plugins/animator/interface/intf_animation.h>

namespace velk {

/**
 * @brief Interface for an animator that manages and ticks a set of animations.
 *
 * Animations are added via add() and advanced each frame via tick().
 * Animations remain in the animator until explicitly removed.
 */
class IAnimator : public Interface<IAnimator>
{
public:
    /** @brief Advances all playing animations. */
    virtual void tick(const UpdateInfo& info) = 0;
    /** @brief Adds an animation to be managed. */
    virtual void add(const IAnimation::Ptr& animation) = 0;
    /** @brief Removes an animation from the animator. */
    virtual void remove(const IAnimation::Ptr& animation) = 0;
    /** @brief Stops all animations and removes them. */
    virtual void cancel_all() = 0;
    /** @brief Returns the number of currently playing animations. */
    virtual size_t active_count() const = 0;
    /** @brief Returns the total number of managed animations. */
    virtual size_t count() const = 0;
};

} // namespace velk

#endif // VELK_ANIMATOR_INTF_ANIMATOR_H
