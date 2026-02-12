#ifndef INTF_EVENT_H
#define INTF_EVENT_H

#include <common.h>
#include <interface/intf_function.h>
#include <interface/intf_interface.h>

namespace strata {

/**
 * @brief Interface for an event that supports multiple handler functions.
 *
 * Inherits IFunction, so an event can be invoked directly. Handlers can be
 * added/removed, and each handler is registered as either Immediate (called
 * synchronously when the event fires) or Deferred (queued for the next update() call).
 */
class IEvent : public Interface<IEvent, IFunction>
{
public:
    /**
     * @brief Adds a handler function for the event.
     * @param fn Handler to register.
     * @param type Immediate handlers fire synchronously; Deferred handlers are queued for update().
     */
    virtual ReturnValue add_handler(const IFunction::ConstPtr &fn, InvokeType type = Immediate) const = 0;
    /**
     * @brief Removes an event handler function.
     * @param fn Handler to remove.
     * @param type Must match the InvokeType used when the handler was added.
     */
    virtual ReturnValue remove_handler(const IFunction::ConstPtr &fn, InvokeType type = Immediate) const = 0;
};

/**
 * @brief Invokes an event with null safety.
 * @param event Event to invoke.
 * @param args Arguments for invocation.
 */
[[maybe_unused]] static ReturnValue invoke_event(const IEvent::ConstPtr &event, const IAny *args)
{
    return event ? event->invoke(args) : ReturnValue::INVALID_ARGUMENT;
}

/** @copydoc invoke_event */
[[maybe_unused]] static ReturnValue invoke_event(const IEvent::Ptr &event, const IAny *args)
{
    return event ? event->invoke(args) : ReturnValue::INVALID_ARGUMENT;
}

} // namespace strata

#endif // INTF_EVENT_H
