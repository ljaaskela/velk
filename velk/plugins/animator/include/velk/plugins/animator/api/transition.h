#ifndef VELK_ANIMATOR_API_TRANSITION_H
#define VELK_ANIMATOR_API_TRANSITION_H

#include <velk/api/object.h>
#include <velk/plugins/animator/interface/intf_transition.h>

namespace velk {

/** @brief Typed wrapper around an ITransition, inheriting Object. */
class Transition : public Object
{
public:
    Transition() = default;
    explicit Transition(IObject::Ptr obj) : Object(std::move(obj)) {}
    explicit Transition(ITransition::Ptr t)
        : Object(t ? interface_pointer_cast<IObject>(t) : IObject::Ptr{}),
          transition_(t.get())
    {}

    /** @brief Returns true if the transition is currently animating. */
    bool is_animating() const
    {
        auto* t = intf();
        return t && t->animating().get_value();
    }

    /** @brief Returns the transition duration, or zero if invalid. */
    Duration get_duration() const
    {
        auto* t = intf();
        return t ? t->duration().get_value() : Duration{};
    }

    /** @brief Sets the transition duration. */
    void set_duration(Duration d)
    {
        if (auto* t = intf()) {
            t->duration().set_value(d);
        }
    }

    /** @brief Sets the easing function. */
    void set_easing(easing::EasingFn e)
    {
        if (auto* t = intf()) {
            t->set_easing(e);
        }
    }

    /** @brief Installs this transition on a property target. */
    void add_target(const IProperty::Ptr& target)
    {
        if (auto* t = intf()) {
            t->add_target(target);
        }
    }

    /** @brief Installs this transition on a typed property target. */
    template <class T>
    void add_target(Property<T> target)
    {
        add_target(target.get_property_interface());
    }

    /** @brief Removes this transition from a property target. */
    void remove_target(const IProperty::Ptr& target)
    {
        if (auto* t = intf()) {
            t->remove_target(target);
        }
    }

    /** @brief Removes this transition from a typed property target. */
    template <class T>
    void remove_target(Property<T> target)
    {
        remove_target(target.get_property_interface());
    }

    /** @brief Detaches from all properties and releases the handle. */
    void remove()
    {
        if (auto* t = intf()) {
            t->uninstall();
        }
        *this = Transition{};
    }

    /** @brief Returns the underlying ITransition as a shared pointer. */
    ITransition::Ptr get_transition_interface() const { return as_ptr<ITransition>(); }

private:
    ITransition* intf() const
    {
        if (!transition_ && get()) {
            transition_ = interface_cast<ITransition>(get());
        }
        return transition_;
    }

    mutable ITransition* transition_ = nullptr;
};

} // namespace velk

#endif // VELK_ANIMATOR_API_TRANSITION_H
