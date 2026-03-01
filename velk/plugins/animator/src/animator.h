#ifndef VELK_ANIMATOR_IMPL_H
#define VELK_ANIMATOR_IMPL_H

#include <velk/ext/object.h>
#include <velk/plugins/animator/interface/intf_animator.h>
#include <velk/plugins/animator/plugin.h>
#include <velk/vector.h>

namespace velk {

class AnimatorImpl : public ext::Object<AnimatorImpl, IAnimator>
{
public:
    VELK_CLASS_UID(ClassId::Animator);

    void tick(const UpdateInfo& info) override;
    void add(const IAnimation::Ptr& animation) override;
    void cancel_all() override;
    size_t active_count() const override;

private:
    vector<IAnimation::Ptr> animations_;
};

} // namespace velk

#endif // VELK_ANIMATOR_IMPL_H
