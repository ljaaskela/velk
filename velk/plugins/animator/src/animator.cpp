#include "animator.h"

#include <velk/plugins/animator/interface/intf_animation.h>

namespace velk {

void AnimatorImpl::tick(const UpdateInfo& info)
{
    for (size_t i = 0; i < animations_.size();) {
        auto* anim = animations_[i].get();
        if (!anim || anim->tick(info)) {
            animations_.erase(animations_.begin() + static_cast<ptrdiff_t>(i));
        } else {
            ++i;
        }
    }
}

void AnimatorImpl::add(const IAnimation::Ptr& animation)
{
    if (animation) {
        animations_.push_back(animation);
    }
}

void AnimatorImpl::cancel_all()
{
    for (auto& anim : animations_) {
        if (anim) {
            anim->cancel();
        }
    }
    animations_.clear();
}

size_t AnimatorImpl::active_count() const
{
    return animations_.size();
}

} // namespace velk
