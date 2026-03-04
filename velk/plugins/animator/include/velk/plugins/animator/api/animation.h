#ifndef VELK_ANIMATOR_ANIMATION_H
#define VELK_ANIMATOR_ANIMATION_H

#include <velk/api/any.h>
#include <velk/api/object.h>
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

/** @brief Typed wrapper around an IAnimation, inheriting Object. */
class Animation : public Object
{
public:
    Animation() = default;
    explicit Animation(IObject::Ptr obj) : Object(std::move(obj)) {}
    explicit Animation(IAnimation::Ptr anim)
        : Object(anim ? interface_pointer_cast<IObject>(anim) : IObject::Ptr{}),
          anim_(anim.get())
    {}

    /** @brief Returns true if the animation has run to completion, or is null. */
    bool is_finished() const
    {
        auto* a = intf();
        return !a || a->state().get_value() == PlayState::Finished;
    }

    /** @brief Returns true if the animation is currently playing. */
    bool is_playing() const
    {
        auto* a = intf();
        return a && a->state().get_value() == PlayState::Playing;
    }

    /** @brief Returns true if the animation is paused. */
    bool is_paused() const
    {
        auto* a = intf();
        return a && a->state().get_value() == PlayState::Paused;
    }

    /** @brief Returns true if the animation is idle (not yet started, or reset via stop()). */
    bool is_idle() const
    {
        auto* a = intf();
        return !a || a->state().get_value() == PlayState::Idle;
    }

    /** @brief Starts or resumes playback. */
    void play()
    {
        if (auto* a = intf()) {
            a->play();
        }
    }

    /** @brief Pauses playback, preserving position. */
    void pause()
    {
        if (auto* a = intf()) {
            a->pause();
        }
    }

    /** @brief Stops the animation and resets to the beginning (Idle). */
    void stop()
    {
        if (auto* a = intf()) {
            a->stop();
        }
    }

    /** @brief Jumps to the end, applies the final value (Finished). */
    void finish()
    {
        if (auto* a = intf()) {
            a->finish();
        }
    }

    /** @brief Resets to beginning and starts playback. */
    void restart()
    {
        if (auto* a = intf()) {
            a->restart();
        }
    }

    /** @brief Seeks to a normalized position (0..1). */
    void seek(float progress)
    {
        if (auto* a = intf()) {
            a->seek(progress);
        }
    }

    /** @brief Returns the total duration, or zero if invalid. */
    Duration get_duration() const
    {
        auto* a = intf();
        return a ? a->duration().get_value() : Duration{};
    }

    /** @brief Returns the elapsed time, or zero if invalid. */
    Duration get_elapsed() const
    {
        auto* a = intf();
        return a ? a->elapsed().get_value() : Duration{};
    }

    /** @brief Returns the normalized progress (0..1), or 0 if invalid. */
    float get_progress() const
    {
        auto* a = intf();
        return a ? a->progress().get_value() : 0.f;
    }

    /** @brief Installs this animation on a property target. */
    void add_target(const IProperty::Ptr& target)
    {
        if (auto* a = intf()) {
            a->add_target(target);
        }
    }

    /** @brief Installs this animation on a typed property target. */
    template <class T>
    void add_target(Property<T> target)
    {
        add_target(target.get_property_interface());
    }

    /** @brief Removes this animation from a property target. */
    void remove_target(const IProperty::Ptr& target)
    {
        if (auto* a = intf()) {
            a->remove_target(target);
        }
    }

    /** @brief Removes this animation from a typed property target. */
    template <class T>
    void remove_target(Property<T> target)
    {
        remove_target(target.get_property_interface());
    }

    /** @brief Returns the underlying IAnimation as a shared pointer. */
    IAnimation::Ptr get_animation_interface() const { return as_ptr<IAnimation>(); }

    /** @brief Sets typed keyframes on the animation. */
    template <class T>
    void set_keyframes(array_view<Keyframe<T>> keyframes)
    {
        auto* a = intf();
        if (a) {
            vector<KeyframeEntry> entries;
            entries.reserve(keyframes.size());
            for (auto& kf : keyframes) {
                Any<T> val(kf.value);
                if (val) {
                    entries.push_back(
                        {kf.time, val.get_any_interface()->template get_self<IAny>(), kf.easing});
                }
            }
            a->set_keyframes(entries);
        }
    }

private:
    IAnimation* intf() const
    {
        if (!anim_ && get()) {
            anim_ = interface_cast<IAnimation>(get());
        }
        return anim_;
    }

    mutable IAnimation* anim_ = nullptr;
};

} // namespace velk

#endif // VELK_ANIMATOR_ANIMATION_H
