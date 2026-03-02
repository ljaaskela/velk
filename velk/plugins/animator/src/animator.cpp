#include "animator.h"

#include <velk/plugins/animator/interface/intf_animation.h>

namespace velk {

void AnimatorImpl::tick(const UpdateInfo& info)
{
    for (auto& anim : animations_) {
        if (anim && anim->state().get_value() == PlayState::Playing) {
            anim->tick(info);
        }
    }
}

void AnimatorImpl::add(const IAnimation::Ptr& animation)
{
    if (animation) {
        animations_.push_back(animation);
    }
}

void AnimatorImpl::remove(const IAnimation::Ptr& animation)
{
    if (!animation) {
        return;
    }
    for (size_t i = 0; i < animations_.size(); ++i) {
        if (animations_[i].get() == animation.get()) {
            animations_.erase(animations_.begin() + static_cast<ptrdiff_t>(i));
            return;
        }
    }
}

void AnimatorImpl::cancel_all()
{
    for (auto& anim : animations_) {
        if (anim) {
            anim->stop();
        }
    }
    animations_.clear();
}

size_t AnimatorImpl::active_count() const
{
    size_t n = 0;
    for (auto& anim : animations_) {
        if (anim && anim->state().get_value() == PlayState::Playing) {
            ++n;
        }
    }
    return n;
}

size_t AnimatorImpl::count() const
{
    return animations_.size();
}

} // namespace velk
