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
    if (!s->keyframes.empty() && has_targets()) {
        write_value(*s->keyframes.back().value);
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

    // Resolve type info and interpolator from first target's inner
    if (!targets_.empty() && targets_[0].inner) {
        auto& inner = targets_[0].inner;
        auto types = inner->get_compatible_types();
        if (!types.empty()) {
            typeUid_ = types[0];
            interpolator_ = instance().type_registry().find_interpolator(typeUid_);
        }
        result_ = inner->clone();
    }
    sorted_ = true;
}

void AnimationImpl::apply_at(IAnimation::State& s, float global_t)
{
    if (s.keyframes.size() < 2 || !has_targets()) {
        if (!s.keyframes.empty() && has_targets()) {
            write_value(*s.keyframes.front().value);
        }
        return;
    }

    if (global_t >= 1.f) {
        if (s.keyframes.back().value) {
            write_value(*s.keyframes.back().value);
        }
        return;
    }
    if (global_t <= 0.f) {
        if (s.keyframes.front().value) {
            write_value(*s.keyframes.front().value);
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
            write_value(*s.keyframes.back().value);
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
        write_value(*result_);
    }
}

void AnimationImpl::write_value(const IAny& value)
{
    if (display_) {
        display_->copy_from(value);
    }
    for (auto& entry : targets_) {
        if (entry.inner) {
            entry.inner->copy_from(value);
        }
        auto prop = entry.owner.lock();
        if (prop) {
            auto pi = interface_pointer_cast<IPropertyInternal>(prop);
            if (pi) {
                instance().queue_deferred_property({pi, nullptr});
            }
        }
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
        if (!s.keyframes.empty() && has_targets()) {
            write_value(*s.keyframes.front().value);
        }
        s.state = PlayState::Finished;
        s.progress = 1.f;
        notify_state(s);
        return false;
    }

    ensure_init(s);

    // Set initial value on first tick
    if (s.elapsed.us == 0 && has_targets() && s.keyframes.front().value) {
        write_value(*s.keyframes.front().value);
    }

    if (dt.us) {
        s.elapsed.us += dt.us;
    }

    if (s.elapsed.us >= s.duration.us) {
        s.elapsed = s.duration;
        s.progress = 1.f;
        if (has_targets() && s.keyframes.back().value) {
            write_value(*s.keyframes.back().value);
        }
        s.state = PlayState::Finished;
        notify_state(s);
        return false;
    }

    s.progress =
        (s.duration.us > 0) ? static_cast<float>(s.elapsed.us) / static_cast<float>(s.duration.us) : 0.f;

    // Find the segment: first keyframe with time > elapsed
    size_t i = 1;
    while (i < s.keyframes.size() && s.keyframes[i].time.us <= s.elapsed.us) {
        ++i;
    }

    if (i >= s.keyframes.size()) {
        if (has_targets() && s.keyframes.back().value) {
            write_value(*s.keyframes.back().value);
        }
        s.state = PlayState::Finished;
        s.progress = 1.f;
        notify_state(s);
        return false;
    }

    if (has_targets() && interpolator_ && result_ && s.keyframes[i - 1].value && s.keyframes[i].value) {
        auto& kf0 = s.keyframes[i - 1];
        auto& kf1 = s.keyframes[i];
        int64_t seg_duration = kf1.time.us - kf0.time.us;
        float seg_t = (seg_duration > 0)
                          ? static_cast<float>(s.elapsed.us - kf0.time.us) / static_cast<float>(seg_duration)
                          : 1.f;
        float eased_t = kf1.easing(seg_t);
        interpolator_(*kf0.value, *kf1.value, eased_t, *result_);
        write_value(*result_);
    }

    notify_state(s);
    return false;
}

// IAnyExtension

IAny::ConstPtr AnimationImpl::get_inner() const
{
    return targets_.empty() ? nullptr : targets_[0].inner;
}

void AnimationImpl::set_inner(IAny::Ptr inner, const IInterface::WeakPtr& owner)
{
    // First target entry: initialize display/result/interpolator
    if (targets_.empty() && inner) {
        display_ = inner->clone();
        result_ = inner->clone();

        auto types = inner->get_compatible_types();
        if (!types.empty()) {
            typeUid_ = types[0];
            interpolator_ = instance().type_registry().find_interpolator(typeUid_);
        }
    }
    targets_.push_back({owner, std::move(inner)});
}

IAny::Ptr AnimationImpl::take_inner(IInterface& owner)
{
    for (auto it = targets_.begin(); it != targets_.end(); ++it) {
        auto locked = it->owner.lock();
        if (locked && locked.get() == &owner) {
            auto inner = std::move(it->inner);
            targets_.erase(it);
            if (targets_.empty()) {
                display_ = nullptr;
                result_ = nullptr;
                interpolator_ = nullptr;
            }
            return inner;
        }
    }
    return nullptr;
}

// IAnimation: add_target / remove_target

void AnimationImpl::add_target(const IProperty::Ptr& target)
{
    auto* pi = interface_cast<IPropertyInternal>(target.get());
    if (pi) {
        pi->install_extension(get_self<IAnyExtension>());
    }
}

void AnimationImpl::remove_target(const IProperty::Ptr& target)
{
    auto* pi = interface_cast<IPropertyInternal>(target.get());
    if (pi) {
        pi->remove_extension(get_self<IAnyExtension>());
    }
}

// IAny overrides

array_view<Uid> AnimationImpl::get_compatible_types() const
{
    return display_ ? display_->get_compatible_types() : array_view<Uid>{};
}

size_t AnimationImpl::get_data_size(Uid type) const
{
    return display_ ? display_->get_data_size(type) : 0;
}

ReturnValue AnimationImpl::get_data(void* to, size_t toSize, Uid type) const
{
    auto* s = const_cast<AnimationImpl*>(this)->state();
    if (s && (s->state == PlayState::Playing || s->state == PlayState::Paused)) {
        return display_ ? display_->get_data(to, toSize, type) : ReturnValue::Fail;
    }
    return (!targets_.empty() && targets_[0].inner) ? targets_[0].inner->get_data(to, toSize, type)
                                                    : ReturnValue::Fail;
}

ReturnValue AnimationImpl::set_data(void const* from, size_t fromSize, Uid type)
{
    ReturnValue ret = ReturnValue::Fail;
    for (auto& entry : targets_) {
        if (entry.inner) {
            auto r = entry.inner->set_data(from, fromSize, type);
            if (succeeded(r)) {
                ret = r;
            }
        }
    }
    if (display_ && succeeded(ret)) {
        display_->set_data(from, fromSize, type);
    }
    return ret;
}

ReturnValue AnimationImpl::copy_from(const IAny& other)
{
    ReturnValue ret = ReturnValue::Fail;
    for (auto& entry : targets_) {
        if (entry.inner) {
            auto r = entry.inner->copy_from(other);
            if (succeeded(r)) {
                ret = r;
            }
        }
    }
    if (display_ && succeeded(ret)) {
        display_->copy_from(other);
    }
    return ret;
}

IAny::Ptr AnimationImpl::clone() const
{
    return display_ ? display_->clone()
                    : ((!targets_.empty() && targets_[0].inner) ? targets_[0].inner->clone() : nullptr);
}

} // namespace velk
