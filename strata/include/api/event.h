#ifndef API_EVENT_H
#define API_EVENT_H

#include <api/callback.h>
#include <common.h>
#include <interface/intf_event.h>
#include <interface/intf_function.h>

#include <type_traits>

namespace strata {

/** @brief Lightweight wrapper around an existing IEvent pointer. */
class Event
{
public:
    /** @brief Wraps an existing IEvent pointer. */
    explicit Event(IEvent::Ptr existing) : event_(std::move(existing)) {}

    /** @brief Implicit conversion to IEvent::Ptr. */
    operator IEvent::Ptr() { return event_; }
    operator const IEvent::ConstPtr() const { return event_; }

    /** @brief Returns true if the underlying IEvent is valid. */
    operator bool() const { return event_.operator bool(); }

    /** @brief Returns the underlying IEvent pointer. */
    IEvent::Ptr get_event_interface() { return event_; }
    /** @copydoc get_event_interface() */
    IEvent::ConstPtr get_event_interface() const { return event_; }

    /** @brief Adds a handler function for the event (null-safe). */
    ReturnValue add_handler(const IFunction::ConstPtr& fn, InvokeType type = Immediate) const
    {
        return event_ ? event_->add_handler(fn, type) : ReturnValue::INVALID_ARGUMENT;
    }

    /**
     * @brief Adds a lambda handler for the event (null-safe).
     *
     * Accepts a callable (lambda, functor, etc.) and automatically wraps it in a Callback
     * before registering it as a handler. The Callback is created inline and implicitly
     * converted to IFunction::ConstPtr.
     *
     * @tparam F Callable type (lambda, functor, function pointer)
     * @param callable The callable to wrap and register
     * @param type Immediate or Deferred handler execution
     *
     * @par Example
     * @code
     * Event evt = widget->on_clicked();
     * evt.add_handler([](FnArgs args) { std::cout << "clicked!\n"; return SUCCESS; });
     * evt.add_handler([](int x, float y) { std::cout << x + y << std::endl; });
     * @endcode
     */
    template<class F, detail::require<!std::is_convertible_v<F, const IFunction::ConstPtr&>> = 0>
    ReturnValue add_handler(F&& callable, InvokeType type = Immediate) const
    {
        Callback cb(std::forward<F>(callable));
        return add_handler(cb, type);
    }

    /** @brief Removes an event handler function (null-safe). */
    ReturnValue remove_handler(const IFunction::ConstPtr& fn) const
    {
        return event_ ? event_->remove_handler(fn) : ReturnValue::INVALID_ARGUMENT;
    }

    /** @brief Returns true if the event has any handlers (null-safe). */
    bool has_handlers() const
    {
        return event_ ? event_->has_handlers() : false;
    }

    /** @brief Invokes the event with no arguments (null-safe). */
    ReturnValue invoke(InvokeType type = Immediate) const
    {
        if (!event_) return ReturnValue::INVALID_ARGUMENT;
        event_->invoke({}, type);
        return ReturnValue::SUCCESS;
    }

    /** @brief Invokes the event with the given @p args (null-safe). */
    ReturnValue invoke(FnArgs args, InvokeType type = Immediate) const
    {
        if (!event_) return ReturnValue::INVALID_ARGUMENT;
        event_->invoke(args, type);
        return ReturnValue::SUCCESS;
    }

private:
    IEvent::Ptr event_;
};

} // namespace strata

#endif // API_EVENT_H
