#ifndef VELK_ANIMATOR_ANIMATOR_H
#define VELK_ANIMATOR_ANIMATOR_H

#include <velk/api/any.h>
#include <velk/api/property.h>
#include <velk/api/traits.h>
#include <velk/api/velk.h>
#include <velk/array_view.h>
#include <velk/plugins/animator/api/animation.h>
#include <velk/plugins/animator/api/transition.h>
#include <velk/plugins/animator/interface/intf_animator.h>
#include <velk/plugins/animator/interface/intf_animator_plugin.h>
#include <velk/plugins/animator/plugin.h>
#include <velk/vector.h>

namespace velk {

namespace detail {

inline IAnimation::Ptr animation(const IProperty::Ptr& target, Uid classId)
{
    auto obj = instance().create<IObject>(classId);
    auto anim = interface_pointer_cast<IAnimation>(obj);
    if (anim) {
        auto* pi = interface_cast<IPropertyInternal>(target.get());
        if (pi) {
            pi->install_extension(interface_pointer_cast<IAnyExtension>(anim));
        }
    }
    return anim;
}

inline IAnimation::Ptr animation(Uid classId)
{
    auto obj = instance().create<IObject>(classId);
    return interface_pointer_cast<IAnimation>(obj);
}

inline IAnimation::Ptr tween(const IProperty::Ptr& target, const IAny& from, const IAny& to,
                              Duration duration, easing::EasingFn ease = easing::linear)
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

inline IAnimation::Ptr tween(const IAny& from, const IAny& to, Duration duration,
                              easing::EasingFn ease = easing::linear)
{
    auto anim = animation(ClassId::Animation);
    if (anim) {
        KeyframeEntry kfs[] = {
            {{}, from.clone(), easing::linear},
            {duration, to.clone(), ease},
        };
        anim->set_keyframes({kfs, 2});
    }
    return anim;
}

inline ITransition::Ptr transition(const IProperty::Ptr& target, Duration duration,
                                    easing::EasingFn ease = easing::linear)
{
    auto obj = instance().create<IObject>(ClassId::Transition);
    auto tr = interface_pointer_cast<ITransition>(obj);
    if (tr) {
        tr->set_easing(ease);
        tr->duration().set_value(duration);
        auto* pi = interface_cast<IPropertyInternal>(target.get());
        if (pi) {
            pi->install_extension(interface_pointer_cast<IAnyExtension>(tr));
        }
    }
    return tr;
}

} // namespace detail

/** @brief Creates a tween animation with a target, adds it to @p animator, and auto-plays. */
template <class T>
Animation create_tween(IAnimator& animator, Property<T> target, T from, T to, Duration duration,
                       easing::EasingFn ease = easing::linear)
{
    Any<T> fromAny(from);
    Any<T> toAny(to);
    auto anim = detail::tween(target, fromAny, toAny, duration, ease);
    if (anim) {
        animator.add(anim);
        anim->play();
        return Animation(anim);
    }
    return {};
}

/** @brief Creates a targetless tween animation, adds it to @p animator (idle, call add_target + play). */
template <class T>
Animation create_tween(IAnimator& animator, T from, T to, Duration duration,
                       easing::EasingFn ease = easing::linear)
{
    Any<T> fromAny(from);
    Any<T> toAny(to);
    auto anim = detail::tween(fromAny, toAny, duration, ease);
    if (anim) {
        animator.add(anim);
        return Animation(anim);
    }
    return {};
}

/** @brief Creates a tween from the property's current value, adds it to @p animator, and auto-plays. */
template <class T>
Animation create_tween_to(IAnimator& animator, Property<T> target, T to, Duration duration,
                          easing::EasingFn ease = easing::linear)
{
    return create_tween(animator, target, target.get_value(), to, duration, ease);
}

/** @brief Creates a multi-keyframe animation track with a target, adds it to @p animator, and auto-plays. */
template <class T>
Animation create_track(IAnimator& animator, Property<T> target,
                       detail::non_deduced_t<array_view<Keyframe<T>>> keyframes)
{
    auto anim = detail::animation(target, ClassId::Animation);
    if (anim) {
        Animation h(anim);
        h.set_keyframes(keyframes);
        animator.add(anim);
        anim->play();
        return h;
    }
    return {};
}

/** @brief Creates a targetless multi-keyframe animation track, adds it to @p animator (idle). */
template <class T>
Animation create_track(IAnimator& animator, detail::non_deduced_t<array_view<Keyframe<T>>> keyframes)
{
    auto anim = detail::animation(ClassId::Animation);
    if (anim) {
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

/** @brief Creates and installs an implicit transition on a property. Drop the handle to remove. */
template <class T>
Transition create_transition(Property<T> target, Duration duration, easing::EasingFn ease = easing::linear)
{
    // Ensure the plugin is loaded (registers TransitionImpl type)
    get_or_load_plugin<IAnimatorPlugin>(PluginId::AnimatorPlugin);
    auto tr = detail::transition(target.get_property_interface(), duration, ease);
    return tr ? Transition(tr) : Transition{};
}

/** @brief Creates a targetless transition (use add_target to apply to properties). */
inline Transition create_transition(Duration duration, easing::EasingFn ease = easing::linear)
{
    get_or_load_plugin<IAnimatorPlugin>(PluginId::AnimatorPlugin);
    auto obj = instance().create<IObject>(ClassId::Transition);
    auto tr = interface_pointer_cast<ITransition>(obj);
    if (tr) {
        tr->set_easing(ease);
        tr->duration().set_value(duration);
    }
    return tr ? Transition(tr) : Transition{};
}

} // namespace velk

#endif // VELK_ANIMATOR_ANIMATOR_H
