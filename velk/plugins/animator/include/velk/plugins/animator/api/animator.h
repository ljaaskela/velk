#ifndef VELK_ANIMATOR_ANIMATOR_H
#define VELK_ANIMATOR_ANIMATOR_H

#include <velk/api/any.h>
#include <velk/api/property.h>
#include <velk/api/traits.h>
#include <velk/api/velk.h>
#include <velk/array_view.h>
#include <velk/plugins/animator/api/animation.h>
#include <velk/plugins/animator/interface/intf_animator.h>
#include <velk/plugins/animator/interface/intf_animator_plugin.h>
#include <velk/plugins/animator/lerp_traits.h>
#include <velk/plugins/animator/plugin.h>
#include <velk/vector.h>

namespace velk {

namespace detail {

/** @brief Typed lerp callback bridging Property<T> to IAnimation's type-erased lerp. */
template <class T>
void typed_lerp(const void* from, const void* to, float t, void* result)
{
    const auto& a = *static_cast<const T*>(from);
    const auto& b = *static_cast<const T*>(to);
    *static_cast<T*>(result) = lerp_trait<T>::lerp(a, b, t);
}

IAnimation::Ptr animation(const IProperty::Ptr& target, Uid classId)
{
    auto obj = instance().create<IObject>(classId);
    auto anim = interface_pointer_cast<IAnimation>(obj);
    if (anim) {
        anim->set_target(target);
    }
    return anim;
}

IAnimation::Ptr tween(const IProperty::Ptr& target, const IAny& from, const IAny& to, Duration duration,
                      easing::EasingFn ease = easing::linear)
{
    auto anim = animation(target, ClassId::Animation);
    if (anim) {
        KeyframeEntry kfs[] = {
            {{}, from.clone(), easing::linear},
            {duration, to.clone(), ease},
        };
        anim->set_keyframes({kfs, 2});
    }
    return anim;
}

} // namespace detail

/** @brief Creates a tween animation, adds it to @p animator, and returns a handle. */
template <class T>
Animation tween(IAnimator& animator, Property<T> target, T from, T to, Duration duration,
                easing::EasingFn ease = easing::linear)
{
    Any<T> fromAny(from);
    Any<T> toAny(to);
    auto anim = detail::tween(target, fromAny, toAny, duration, ease);
    if (anim) {
        anim->set_lerp(&detail::typed_lerp<T>);
        animator.add(anim);
        return Animation(anim);
    }
    return {};
}

/** @brief Creates a tween from the property's current value, adds it to @p animator. */
template <class T>
Animation tween_to(IAnimator& animator, Property<T> target, T to, Duration duration,
                   easing::EasingFn ease = easing::linear)
{
    return tween(animator, target, target.get_value(), to, duration, ease);
}

/** @brief Creates a multi-keyframe animation track, adds it to @p animator. */
template <class T>
Animation track(IAnimator& animator, Property<T> target,
                detail::non_deduced_t<array_view<Keyframe<T>>> keyframes)
{
    auto anim = detail::animation(target, ClassId::Animation);
    if (anim) {
        anim->set_lerp(&detail::typed_lerp<T>);
        Animation h(anim);
        h.set_keyframes(keyframes);
        animator.add(anim);
        return h;
    }
    return {};
}

/**
 * @brief Returns the default animator managed by the animator plugin.
 *
 * The returned animator is ticked automatically during instance().update().
 */
inline IAnimator& default_animator()
{
    auto* ap = get_or_load_plugin<IAnimatorPlugin>(PluginId::AnimatorPlugin);
    assert(ap);
    return ap->get_default_animator();
}

} // namespace velk

#endif // VELK_ANIMATOR_ANIMATOR_H
