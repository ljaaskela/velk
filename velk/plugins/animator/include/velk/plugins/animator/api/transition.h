#ifndef VELK_ANIMATOR_API_TRANSITION_H
#define VELK_ANIMATOR_API_TRANSITION_H

#include <velk/plugins/animator/interface/intf_transition.h>

namespace velk {

/** @brief Typed wrapper around an ITransition. Holds a strong reference. */
class Transition
{
public:
    Transition() = default;
    explicit Transition(ITransition::Ptr t) : transition_(std::move(t)) {}

    /** @brief Returns true if the transition is currently animating. */
    bool is_animating() const
    {
        return transition_ && transition_->animating().get_value();
    }

    /** @brief Returns the transition duration, or zero if invalid. */
    Duration get_duration() const
    {
        return transition_ ? transition_->duration().get_value() : Duration{};
    }

    /** @brief Sets the transition duration. */
    void set_duration(Duration d)
    {
        if (transition_) {
            transition_->duration().set_value(d);
        }
    }

    /** @brief Sets the easing function. */
    void set_easing(easing::EasingFn e)
    {
        if (transition_) {
            transition_->set_easing(e);
        }
    }

    /** @brief Uninstalls and releases the transition. */
    void remove() { transition_ = nullptr; }

    /** @brief Returns the underlying ITransition pointer. */
    ITransition::Ptr get_transition_interface() { return transition_; }

    /** @copydoc get_transition_interface() */
    const ITransition::ConstPtr get_transition_interface() const { return transition_; }

    /** @brief Returns true if the transition is valid. */
    operator bool() const { return transition_.operator bool(); }

private:
    ITransition::Ptr transition_;
};

} // namespace velk

#endif // VELK_ANIMATOR_API_TRANSITION_H
