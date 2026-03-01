#include "animated_any.h"

#include <velk/interface/intf_event.h>

#include <algorithm>

namespace velk {

void AnimatedAny::init(IAny::Ptr original, const TransitionConfig& config,
                       InterpolatorFn interpolator, const IProperty::Ptr& property)
{
    original_ = std::move(original);
    config_ = config;
    interpolator_ = interpolator;
    if (property) {
        property_ = IProperty::WeakPtr(property);
    }

    // Create display as a clone of original (reads go through display)
    display_ = original_->clone();
    // Pre-allocate scratch buffers
    from_ = original_->clone();
    target_ = original_->clone();
    result_ = original_->clone();
}

bool AnimatedAny::tick(Duration dt)
{
    if (!animating_) {
        return false;
    }

    elapsed_.us += dt.us;

    float t = 1.f;
    if (config_.duration.us > 0) {
        t = static_cast<float>(elapsed_.us) / static_cast<float>(config_.duration.us);
        t = std::min(t, 1.f);
    }

    // Apply easing
    float eased = config_.easing ? config_.easing(t) : t;

    // Interpolate from -> target -> result
    if (interpolator_ && from_ && target_ && result_) {
        if (succeeded(interpolator_(*from_, *target_, eased, *result_))) {
            // Write interpolated value to display and original
            display_->copy_from(*result_);
            original_->copy_from(*result_);
        }
    }

    // Fire on_changed on the property
    auto prop = property_.lock();
    if (prop && prop->on_changed()) {
        invoke_event(prop->on_changed(), display_.get());
    }

    // Check if finished
    if (t >= 1.f) {
        animating_ = false;
        return false;
    }
    return true;
}

IAny::Ptr AnimatedAny::take_original()
{
    // Copy current display value to original before returning
    if (original_ && display_) {
        original_->copy_from(*display_);
    }
    return std::move(original_);
}

// IAny

array_view<Uid> AnimatedAny::get_compatible_types() const
{
    return display_ ? display_->get_compatible_types() : array_view<Uid>{};
}

size_t AnimatedAny::get_data_size(Uid type) const
{
    return display_ ? display_->get_data_size(type) : 0;
}

ReturnValue AnimatedAny::get_data(void* to, size_t toSize, Uid type) const
{
    return display_ ? display_->get_data(to, toSize, type) : ReturnValue::Fail;
}

ReturnValue AnimatedAny::set_data(void const* from, size_t fromSize, Uid type)
{
    if (!display_ || !interpolator_) {
        // No interpolator: fall through to direct write
        if (display_) {
            auto ret = display_->set_data(from, fromSize, type);
            if (original_ && ret == ReturnValue::Success) {
                original_->set_data(from, fromSize, type);
            }
            return ret;
        }
        return ReturnValue::Fail;
    }

    // Capture from = current display value
    if (from_) {
        from_->copy_from(*display_);
    }

    // Capture target = new value
    if (target_) {
        target_->set_data(from, fromSize, type);
    }

    // Reset animation
    elapsed_ = {};
    animating_ = true;

    // Return NothingToDo so PropertyImpl skips on_changed
    return ReturnValue::NothingToDo;
}

ReturnValue AnimatedAny::copy_from(const IAny& other)
{
    // Passthrough: used by deferred flush (set_value_silent path).
    // Writes directly without animation.
    ReturnValue ret = ReturnValue::Fail;
    if (display_) {
        ret = display_->copy_from(other);
    }
    if (original_ && succeeded(ret)) {
        original_->copy_from(other);
    }
    return ret;
}

IAny::Ptr AnimatedAny::clone() const
{
    // Clone returns a plain value (not an AnimatedAny)
    return display_ ? display_->clone() : nullptr;
}

} // namespace velk
