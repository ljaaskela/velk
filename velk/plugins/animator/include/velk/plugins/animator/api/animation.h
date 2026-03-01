#ifndef VELK_ANIMATOR_ANIMATION_H
#define VELK_ANIMATOR_ANIMATION_H

#include <velk/api/any.h>
#include <velk/plugins/animator/easing.h>
#include <velk/plugins/animator/interface/intf_animation.h>
#include <velk/vector.h>

namespace velk {

/** @brief A single keyframe in an animation track. */
template <class T>
struct Keyframe
{
    Duration time;                            ///< Time from animation start.
    T value;                                  ///< Value at this keyframe.
    easing::EasingFn easing = easing::linear; ///< Easing for the segment arriving at this keyframe.
};

/** @brief Typed wrapper around an IAnimation. Holds a strong reference. */
class Animation
{
public:
    Animation() = default;
    explicit Animation(IAnimation::Ptr anim) : anim_(std::move(anim)) {}

    /** @brief Returns true if the animation has completed or was cancelled. */
    bool is_finished() const { return !anim_ || anim_->finished().get_value(); }

    /** @brief Cancels the animation. */
    void cancel()
    {
        if (anim_) {
            anim_->cancel();
        }
    }

    /** @brief Returns the total duration, or zero if invalid. */
    Duration get_duration() const { return anim_ ? anim_->duration().get_value() : Duration{}; }

    /** @brief Returns the elapsed time, or zero if invalid. */
    Duration get_elapsed() const { return anim_ ? anim_->elapsed().get_value() : Duration{}; }

    /** @brief Returns the underlying IAnimation pointer. */
    IAnimation::Ptr get_animation_interface() { return anim_; }

    /** @copydoc get_animation_interface() */
    const IAnimation::ConstPtr get_animation_interface() const { return anim_; }

    /** @brief Sets typed keyframes on the animation. */
    template <class T>
    void set_keyframes(array_view<Keyframe<T>> keyframes)
    {
        if (anim_) {
            vector<KeyframeEntry> entries;
            entries.reserve(keyframes.size());
            for (auto& kf : keyframes) {
                Any<T> val(kf.value);
                if (val) {
                    entries.push_back(
                        {kf.time, val.get_any_interface()->template get_self<IAny>(), kf.easing});
                }
            }
            anim_->set_keyframes(entries);
        }
    }

    /** @brief Returns true if the animation is valid. */
    operator bool() const { return anim_.operator bool(); }

private:
    IAnimation::Ptr anim_;
};

} // namespace velk

#endif // VELK_ANIMATOR_ANIMATION_H
