#include "animation.h"

#include <velk/api/state.h>
#include <velk/api/velk.h>
#include <velk/interface/intf_event.h>
#include <velk/interface/intf_property.h>

#include <algorithm>

namespace velk {

IAnimation::State* AnimationImpl::state()
{
    return static_cast<IAnimation::State*>(get_property_state(IAnimation::UID));
}

void AnimationImpl::set_keyframes(array_view<KeyframeEntry> keyframes)
{
    auto* s = state();
    if (!s) {
        return;
    }
    s->keyframes.clear();
    s->keyframes.insert(s->keyframes.begin(), keyframes.begin(), keyframes.end());
    sorted_ = false;
}

void AnimationImpl::set_target(const IProperty::Ptr& target)
{
    target_ = target;
}

void AnimationImpl::play()
{
    auto* s = state();
    if (!s) {
        return;
    }
    if (s->state == PlayState::Idle || s->state == PlayState::Finished) {
        s->elapsed = {};
        s->progress = 0.f;
    }
    s->state = PlayState::Playing;
    notify_state(*s);
}

void AnimationImpl::pause()
{
    auto* s = state();
    if (!s || s->state != PlayState::Playing) {
        return;
    }
    s->state = PlayState::Paused;
    notify_state(*s);
}

void AnimationImpl::stop()
{
    auto* s = state();
    if (!s || s->state == PlayState::Idle) {
        return;
    }
    s->elapsed = {};
    s->progress = 0.f;
    s->state = PlayState::Idle;
    notify_state(*s);
}

void AnimationImpl::finish()
{
    auto* s = state();
    if (!s || s->state == PlayState::Finished) {
        return;
    }
    ensure_init(*s);
    if (!s->keyframes.empty() && target_) {
        write_to_target(*s->keyframes.back().value);
    }
    s->elapsed = s->duration;
    s->progress = 1.f;
    s->state = PlayState::Finished;
    notify_state(*s);
}

void AnimationImpl::restart()
{
    auto* s = state();
    if (!s) {
        return;
    }
    s->elapsed = {};
    s->progress = 0.f;
    s->state = PlayState::Playing;
    notify_state(*s);
}

void AnimationImpl::seek(float p)
{
    auto* s = state();
    if (!s) {
        return;
    }
    ensure_init(*s);
    if (p < 0.f) {
        p = 0.f;
    }
    if (p > 1.f) {
        p = 1.f;
    }
    s->elapsed.us = static_cast<int64_t>(static_cast<float>(s->duration.us) * p);
    s->progress = p;
    apply_at(*s, p);
    notify_state(*s);
}

void AnimationImpl::ensure_init(IAnimation::State& s)
{
    if (sorted_) {
        return;
    }
    std::sort(s.keyframes.begin(), s.keyframes.end(), [](const KeyframeEntry& a, const KeyframeEntry& b) {
        return a.time.us < b.time.us;
    });
    if (!s.keyframes.empty()) {
        s.duration = s.keyframes.back().time;
    }

    // Resolve type info and interpolator from target property
    if (target_) {
        auto val = target_->get_value();
        if (val) {
            auto types = val->get_compatible_types();
            if (!types.empty()) {
                typeUid_ = types[0];
                interpolator_ = instance().type_registry().find_interpolator(typeUid_);
            }
            result_ = val->clone();
        }
    }
    sorted_ = true;
}

void AnimationImpl::apply_at(IAnimation::State& s, float global_t)
{
    if (s.keyframes.size() < 2 || !target_) {
        if (!s.keyframes.empty() && target_) {
            write_to_target(*s.keyframes.front().value);
        }
        return;
    }

    if (global_t >= 1.f) {
        if (s.keyframes.back().value) {
            write_to_target(*s.keyframes.back().value);
        }
        return;
    }
    if (global_t <= 0.f) {
        if (s.keyframes.front().value) {
            write_to_target(*s.keyframes.front().value);
        }
        return;
    }

    // Find the segment: first keyframe with time > elapsed
    size_t i = 1;
    while (i < s.keyframes.size() && s.keyframes[i].time.us <= s.elapsed.us) {
        ++i;
    }
    if (i >= s.keyframes.size()) {
        if (s.keyframes.back().value) {
            write_to_target(*s.keyframes.back().value);
        }
        return;
    }

    if (interpolator_ && result_ && s.keyframes[i - 1].value && s.keyframes[i].value) {
        auto& kf0 = s.keyframes[i - 1];
        auto& kf1 = s.keyframes[i];
        int64_t seg_duration = kf1.time.us - kf0.time.us;
        float seg_t = (seg_duration > 0)
                          ? static_cast<float>(s.elapsed.us - kf0.time.us) / static_cast<float>(seg_duration)
                          : 1.f;
        float eased_t = kf1.easing(seg_t);
        interpolator_(*kf0.value, *kf1.value, eased_t, *result_);
        write_to_target(*result_);
    }
}

void AnimationImpl::write_to_target(const IAny& value)
{
    auto pi = interface_pointer_cast<IPropertyInternal>(target_);
    // This is essentially the same as calling target_->set_value(value, Deferred);
    // - But this way we we avoid cloning the value into the deferred task queue.
    // - Animations are usually ticked in instance().update()
    // - We're already anyway just about to set the value
    // - Instead of queuing the set_value, we set the value immediately but queue the property on_changed()
    //   invocation.
    if (pi && pi->set_value_silent(value) == ReturnValue::Success) {
        // Queue notification-only (null value) so on_changed fires during flush
        // alongside other deferred property notifications.
        instance().queue_deferred_property({pi, nullptr});
    }
}

void AnimationImpl::notify_state(IAnimation::State& state)
{
    notify(MemberKind::Property, IAnimation::UID, Notification::Changed);
}

bool AnimationImpl::tick(const UpdateInfo& info)
{
    auto dt = info.dt;
    auto* st = state();

    if (!st || st->state != PlayState::Playing) {
        return false;
    }
    auto& s = *st;

    if (s.keyframes.size() < 2) {
        if (!s.keyframes.empty() && target_) {
            write_to_target(*s.keyframes.front().value);
        }
        s.state = PlayState::Finished;
        s.progress = 1.f;
        notify_state(s);
        return false;
    }

    ensure_init(s);

    // Set initial value on first tick
    if (s.elapsed.us == 0 && target_ && s.keyframes.front().value) {
        write_to_target(*s.keyframes.front().value);
    }

    if (dt.us) {
        s.elapsed.us += dt.us;
    }

    if (s.elapsed.us >= s.duration.us) {
        s.elapsed = s.duration;
        s.progress = 1.f;
        if (target_ && s.keyframes.back().value) {
            write_to_target(*s.keyframes.back().value);
        }
        s.state = PlayState::Finished;
        notify_state(s);
        return false;
    }

    s.progress = (s.duration.us > 0)
                     ? static_cast<float>(s.elapsed.us) / static_cast<float>(s.duration.us)
                     : 0.f;

    // Find the segment: first keyframe with time > elapsed
    size_t i = 1;
    while (i < s.keyframes.size() && s.keyframes[i].time.us <= s.elapsed.us) {
        ++i;
    }

    if (i >= s.keyframes.size()) {
        if (target_ && s.keyframes.back().value) {
            write_to_target(*s.keyframes.back().value);
        }
        s.state = PlayState::Finished;
        s.progress = 1.f;
        notify_state(s);
        return false;
    }

    if (target_ && interpolator_ && result_ && s.keyframes[i - 1].value && s.keyframes[i].value) {
        auto& kf0 = s.keyframes[i - 1];
        auto& kf1 = s.keyframes[i];
        int64_t seg_duration = kf1.time.us - kf0.time.us;
        float seg_t = (seg_duration > 0)
                          ? static_cast<float>(s.elapsed.us - kf0.time.us) / static_cast<float>(seg_duration)
                          : 1.f;
        float eased_t = kf1.easing(seg_t);
        interpolator_(*kf0.value, *kf1.value, eased_t, *result_);
        write_to_target(*result_);
    }

    notify_state(s);
    return false;
}

} // namespace velk
