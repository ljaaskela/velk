#include "animation.h"

#include <velk/api/traits.h>

#include <algorithm>
#include <vector>

namespace velk {

namespace {

template <class T>
void linear_lerp(const void* from, const void* to, float t, void* result)
{
    const auto& a = *static_cast<const T*>(from);
    const auto& b = *static_cast<const T*>(to);
    *static_cast<T*>(result) = static_cast<T>(a + (b - a) * t);
}

LerpFn resolve_lerp(Uid typeUid)
{
    if (typeUid == type_uid<float>()) {
        return &linear_lerp<float>;
    }
    if (typeUid == type_uid<double>()) {
        return &linear_lerp<double>;
    }
    if (typeUid == type_uid<int>()) {
        return &linear_lerp<int>;
    }
    return nullptr;
}

} // namespace

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

void AnimationImpl::set_lerp(LerpFn fn)
{
    lerp_ = fn;
}

void AnimationImpl::cancel()
{
    if (auto* s = state()) {
        s->finished = true;
    }
}

bool AnimationImpl::tick(const UpdateInfo& info)
{
    auto dt = info.dt;

    auto* s = state();
    if (!s) {
        return true;
    }

    if (s->finished) {
        return true;
    }

    if (keyframes_.size() < 2) {
        if (!keyframes_.empty() && target_) {
            target_->set_value(*keyframes_.front().value);
        }
        s->finished = true;
        return true;
    }

    if (!sorted_) {
        std::sort(keyframes_.begin(), keyframes_.end(), [](const KeyframeEntry& a, const KeyframeEntry& b) {
            return a.time.us < b.time.us;
        });
        s->duration = keyframes_.back().time;

        // Resolve type info from target property
        if (target_) {
            auto val = target_->get_value();
            if (val) {
                auto types = val->get_compatible_types();
                if (!types.empty()) {
                    typeUid_ = types[0];
                    valueSize_ = val->get_data_size(typeUid_);
                    if (!lerp_) {
                        lerp_ = resolve_lerp(typeUid_);
                    }
                }
            }
        }

        // Set initial value
        if (target_ && keyframes_.front().value) {
            target_->set_value(*keyframes_.front().value);
        }
        sorted_ = true;
    }

    s->elapsed.us += dt.us;

    if (s->elapsed.us >= s->duration.us) {
        if (target_ && keyframes_.back().value) {
            target_->set_value(*keyframes_.back().value);
        }
        s->finished = true;
        return true;
    }

    // Find the segment: first keyframe with time > elapsed
    size_t i = 1;
    while (i < keyframes_.size() && keyframes_[i].time.us <= s->elapsed.us) {
        ++i;
    }

    if (i >= keyframes_.size()) {
        if (target_ && keyframes_.back().value) {
            target_->set_value(*keyframes_.back().value);
        }
        s->finished = true;
        return true;
    }

    if (target_ && lerp_ && keyframes_[i - 1].value && keyframes_[i].value) {
        auto& kf0 = keyframes_[i - 1];
        auto& kf1 = keyframes_[i];
        int64_t seg_duration = kf1.time.us - kf0.time.us;
        float seg_t = (seg_duration > 0) ? static_cast<float>(s->elapsed.us - kf0.time.us) / static_cast<float>(seg_duration) : 1.f;
        float eased_t = kf1.easing(seg_t);

        std::vector<uint8_t> result(valueSize_);
        std::vector<uint8_t> fromBuf(valueSize_);
        std::vector<uint8_t> toBuf(valueSize_);

        kf0.value->get_data(fromBuf.data(), valueSize_, typeUid_);
        kf1.value->get_data(toBuf.data(), valueSize_, typeUid_);

        lerp_(fromBuf.data(), toBuf.data(), eased_t, result.data());

        if (auto* internal = interface_cast<IPropertyInternal>(target_)) {
            internal->set_data(result.data(), valueSize_, typeUid_);
        }
    }

    return false;
}

} // namespace velk
