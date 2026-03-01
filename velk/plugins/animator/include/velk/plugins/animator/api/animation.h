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

    /** @brief Returns true if the animation has run to completion, or is null. */
    bool is_finished() const { return !anim_ || anim_->state().get_value() == PlayState::Finished; }

    /** @brief Returns true if the animation is currently playing. */
    bool is_playing() const { return anim_ && anim_->state().get_value() == PlayState::Playing; }

    /** @brief Returns true if the animation is paused. */
    bool is_paused() const { return anim_ && anim_->state().get_value() == PlayState::Paused; }

    /** @brief Returns true if the animation is idle (not yet started, or reset via stop()). */
    bool is_idle() const { return !anim_ || anim_->state().get_value() == PlayState::Idle; }

    /** @brief Starts or resumes playback. */
    void play()
    {
        if (anim_) {
            anim_->play();
        }
    }

    /** @brief Pauses playback, preserving position. */
    void pause()
    {
        if (anim_) {
            anim_->pause();
        }
    }

    /** @brief Stops the animation and resets to the beginning (Idle). */
    void stop()
    {
        if (anim_) {
            anim_->stop();
        }
    }

    /** @brief Jumps to the end, applies the final value (Finished). */
    void finish()
    {
        if (anim_) {
            anim_->finish();
        }
    }

    /** @brief Resets to beginning and starts playback. */
    void restart()
    {
        if (anim_) {
            anim_->restart();
        }
    }

    /** @brief Seeks to a normalized position (0..1). */
    void seek(float progress)
    {
        if (anim_) {
            anim_->seek(progress);
        }
    }

    /** @brief Returns the total duration, or zero if invalid. */
    Duration get_duration() const { return anim_ ? anim_->duration().get_value() : Duration{}; }

    /** @brief Returns the elapsed time, or zero if invalid. */
    Duration get_elapsed() const { return anim_ ? anim_->elapsed().get_value() : Duration{}; }

    /** @brief Returns the normalized progress (0..1), or 0 if invalid. */
    float get_progress() const { return anim_ ? anim_->progress().get_value() : 0.f; }

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
