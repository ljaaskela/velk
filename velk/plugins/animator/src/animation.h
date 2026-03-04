#ifndef VELK_ANIMATOR_ANIMATION_IMPL_H
#define VELK_ANIMATOR_ANIMATION_IMPL_H

#include <velk/ext/object.h>
#include <velk/plugins/animator/interface/intf_animation.h>
#include <velk/plugins/animator/plugin.h>
#include <velk/vector.h>

namespace velk {

/**
 * @brief Keyframe-based animation implementation that is itself an IAnyExtension.
 *
 * Installed on one or more properties via install_extension. Drives values from
 * keyframes during tick(), writing directly to all inners and firing on_changed
 * via each owner.
 */
class AnimationImpl : public ext::Object<AnimationImpl, IAnimation, IAnyExtension>
{
public:
    VELK_CLASS_UID(ClassId::Animation);

    // IAnimation
    bool tick(const UpdateInfo& info) override;
    void play() override;
    void pause() override;
    void stop() override;
    void finish() override;
    void restart() override;
    void seek(float progress) override;
    void set_keyframes(array_view<KeyframeEntry> keyframes) override;
    void add_target(const IProperty::Ptr& target) override;
    void remove_target(const IProperty::Ptr& target) override;

    // IAnyExtension
    IAny::ConstPtr get_inner() const override;
    void set_inner(IAny::Ptr inner, const IInterface::WeakPtr& owner) override;
    IAny::Ptr take_inner(IInterface& owner) override;

    // IAny overrides
    array_view<Uid> get_compatible_types() const override;
    size_t get_data_size(Uid type) const override;
    ReturnValue get_data(void* to, size_t toSize, Uid type) const override;
    ReturnValue set_data(void const* from, size_t fromSize, Uid type) override;
    ReturnValue copy_from(const IAny& other) override;
    IAny::Ptr clone() const override;

private:
    struct TargetEntry
    {
        IInterface::WeakPtr owner;
        IAny::Ptr inner;
    };

    IAnimation::State* state();
    void ensure_init(IAnimation::State& state);
    void apply_at(IAnimation::State& s, float t);
    void write_value(const IAny& value);
    void notify_state(IAnimation::State& state);
    bool has_targets() const { return !targets_.empty(); }

    vector<TargetEntry> targets_;
    IAny::Ptr display_;
    InterpolatorFn interpolator_ = nullptr;
    IAny::Ptr result_;
    Uid typeUid_{};
    bool sorted_ = false;
};

} // namespace velk

#endif // VELK_ANIMATOR_ANIMATION_IMPL_H
