#ifndef VELK_ANIMATOR_ANIMATION_IMPL_H
#define VELK_ANIMATOR_ANIMATION_IMPL_H

#include <velk/ext/object.h>
#include <velk/plugins/animator/interface/intf_animation.h>
#include <velk/plugins/animator/plugin.h>

#include <vector>

namespace velk {

/**
 * @brief Keyframe-based animation implementation.
 *
 * Registered with ClassId::Animation. Configured post-construction via
 * set_target and set_keyframes calls.
 * Simple tweens are represented as 2-keyframe tracks.
 */
class AnimationImpl : public ext::Object<AnimationImpl, IAnimation>
{
public:
    VELK_CLASS_UID(ClassId::Animation);

    bool tick(const UpdateInfo& info) override;
    void play() override;
    void pause() override;
    void stop() override;
    void finish() override;
    void restart() override;
    void seek(float progress) override;
    void set_target(const IProperty::Ptr& target) override;
    void set_keyframes(array_view<KeyframeEntry> keyframes) override;

private:
    IAnimation::State* state();
    void ensure_init(IAnimation::State& state);
    void apply_at(IAnimation::State& s, float t);
    void notify_state(IAnimation::State& state);

    IProperty::Ptr target_;
    std::vector<KeyframeEntry> keyframes_;
    InterpolatorFn interpolator_ = nullptr; // resolved from type registry during init
    IAny::Ptr result_;  // cached result buffer, cloned once from first keyframe
    Uid typeUid_{};     // resolved from target on first tick
    bool sorted_ = false;
};

} // namespace velk

#endif // VELK_ANIMATOR_ANIMATION_IMPL_H
