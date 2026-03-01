#include "animation.h"

#include <velk/api/state.h>
#include <velk/api/velk.h>

#include <algorithm>

namespace velk {

IAnimation::State* AnimationImpl::state()
{
    return static_cast<IAnimation::State*>(get_property_state(IAnimation::UID));
}

void AnimationImpl::set_keyframes(array_view<KeyframeEntry> keyframes)
{
    keyframes_.clear();
    keyframes_.reserve(keyframes.size());
    for (auto& kf : keyframes) {
        keyframes_.push_back({kf.time, kf.value, kf.easing});
    }
    sorted_ = false;
}

void AnimationImpl::set_target(const IProperty::Ptr& target)
{
    target_ = target;
}

void AnimationImpl::set_interpolator(InterpolatorFn fn)
{
    interpolator_ = fn;
}

void AnimationImpl::cancel()
{
    if (auto* s = state()) {
        finish(*s);
    }
}

bool AnimationImpl::ensure_init(IAnimation::State& state)
{
    if (sorted_) {
        return false;
    }
    std::sort(keyframes_.begin(), keyframes_.end(), [](const KeyframeEntry& a, const KeyframeEntry& b) {
        return a.time.us < b.time.us;
    });
    auto dur = keyframes_.back().time;
    bool ret = state.duration.us != dur.us;
    if (ret) {
        state.duration = dur;
    }

    // Resolve type info and interpolator from target property
    if (target_) {
        auto val = target_->get_value();
        if (val) {
            auto types = val->get_compatible_types();
            if (!types.empty()) {
                typeUid_ = types[0];
                if (!interpolator_) {
                    interpolator_ = instance().type_registry().find_interpolator(typeUid_);
                }
            }
            result_ = val->clone();
        }
    }

    // Set initial value
    if (target_ && keyframes_.front().value) {
        target_->set_value(*keyframes_.front().value, Deferred);
    }
    sorted_ = true;
    return ret;
}

bool AnimationImpl::finish(IAnimation::State& state)
{
    if (!state.finished) {
        state.finished = true;
        notify_state(state);
    }
    return true;
}

void AnimationImpl::notify_state(IAnimation::State& state)
{
    notify(MemberKind::Property, IAnimation::UID, Notification::Changed);
}

bool AnimationImpl::tick(const UpdateInfo& info)
{
    auto dt = info.dt;
    auto st = state();

    if (!st || st->finished) {
        return true;
    }
    auto& s = *st;

    if (keyframes_.size() < 2) {
        if (!keyframes_.empty() && target_) {
            // Set value as Deferred so that all change handlers run simultaenously once pre_update handled
            // for all plugins
            target_->set_value(*keyframes_.front().value, Deferred);
        }
        return finish(s);
    }

    bool notify = ensure_init(s);

    if (dt.us) {
        s.elapsed.us += dt.us;
        notify = true; // state.elapsed changed
    }

    if (s.elapsed.us >= s.duration.us) {
        if (target_ && keyframes_.back().value) {
            target_->set_value(*keyframes_.back().value, Deferred);
        }
        return finish(s);
    }

    // Find the segment: first keyframe with time > elapsed
    size_t i = 1;
    while (i < keyframes_.size() && keyframes_[i].time.us <= s.elapsed.us) {
        ++i;
    }

    if (i >= keyframes_.size()) {
        if (target_ && keyframes_.back().value) {
            target_->set_value(*keyframes_.back().value, Deferred);
        }
        return finish(s);
    }

    if (target_ && interpolator_ && result_ && keyframes_[i - 1].value && keyframes_[i].value) {
        auto& kf0 = keyframes_[i - 1];
        auto& kf1 = keyframes_[i];
        int64_t seg_duration = kf1.time.us - kf0.time.us;
        float seg_t = (seg_duration > 0)
                          ? static_cast<float>(s.elapsed.us - kf0.time.us) / static_cast<float>(seg_duration)
                          : 1.f;
        float eased_t = kf1.easing(seg_t);
        interpolator_(*kf0.value, *kf1.value, eased_t, *result_);
        target_->set_value(*result_, Deferred);
    }

    if (notify) {
        notify_state(s);
    }

    return false;
}

} // namespace velk
