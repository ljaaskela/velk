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
 * set_target, set_lerp, and repeated add_keyframe calls.
 * Simple tweens are represented as 2-keyframe tracks.
 */
class AnimationImpl : public ext::Object<AnimationImpl, IAnimation>
{
public:
    VELK_CLASS_UID(ClassId::Animation);

    bool tick(const UpdateInfo& info) override;
    void cancel() override;
    void set_target(const IProperty::Ptr& target) override;
    void set_lerp(LerpFn fn) override;
    void set_keyframes(array_view<KeyframeEntry> keyframes) override;

private:
    IAnimation::State* state();

    IProperty::Ptr target_;
    std::vector<KeyframeEntry> keyframes_;
    LerpFn lerp_ = nullptr;
    size_t valueSize_ = 0; // resolved from target on first tick
    Uid typeUid_{};        // resolved from target on first tick
    bool sorted_ = false;
};

} // namespace velk

#endif // VELK_ANIMATOR_ANIMATION_IMPL_H
