#ifndef VELK_ANIMATOR_ANIMATED_ANY_H
#define VELK_ANIMATOR_ANIMATED_ANY_H

#include <velk/ext/core_object.h>
#include <velk/plugins/animator/interface/intf_animated_any.h>
#include <velk/plugins/animator/plugin.h>

namespace velk {

/**
 * @brief Type-erased IAny wrapper that intercepts writes to animate value changes.
 *
 * When installed as a property's backing IAny, set_data captures new values as
 * animation targets and returns NothingToDo so the property skips its on_changed.
 * The AnimatorPlugin ticks all active instances during pre_update, interpolating
 * and firing on_changed via the property.
 */
class AnimatedAny final : public ext::ObjectCore<AnimatedAny, IAnimatedAny>
{
public:
    VELK_CLASS_UID(ClassId::AnimatedAny);

    AnimatedAny() = default;

public: // IAnimatedAny
    void init(IAny::Ptr original, const TransitionConfig& config,
              InterpolatorFn interpolator, const IProperty::Ptr& property) override;
    bool tick(Duration dt) override;
    IAny::Ptr take_original() override;
    bool is_animating() const override { return animating_; }
    void set_config(const TransitionConfig& config) override { config_ = config; }

public: // IAny
    array_view<Uid> get_compatible_types() const override;
    size_t get_data_size(Uid type) const override;
    ReturnValue get_data(void* to, size_t toSize, Uid type) const override;
    ReturnValue set_data(void const* from, size_t fromSize, Uid type) override;
    ReturnValue copy_from(const IAny& other) override;
    IAny::Ptr clone() const override;

private:
    IAny::Ptr original_;     ///< The real backing IAny (AnyRef or AnyValue), keeps State in sync.
    IAny::Ptr display_;      ///< Clone of original, holds current interpolated value for reads.
    IAny::Ptr from_;         ///< Animation start value (snapshot of display at retarget time).
    IAny::Ptr target_;       ///< Animation target value.
    IAny::Ptr result_;       ///< Scratch buffer for interpolator output.
    TransitionConfig config_;
    InterpolatorFn interpolator_ = nullptr;
    Duration elapsed_{};
    bool animating_ = false;
    IProperty::WeakPtr property_;
};

} // namespace velk

#endif // VELK_ANIMATOR_ANIMATED_ANY_H
