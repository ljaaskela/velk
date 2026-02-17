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
     * @param fn Handler to register. A handler can only be added once.
     * @param type Immediate handlers fire synchronously; Deferred handlers are queued for update().
     */
    virtual ReturnValue add_handler(const IFunction::ConstPtr &fn, InvokeType type = Immediate) const = 0;
    /**
     * @brief Removes an event handler function.
     * @param fn Handler to remove (searched in both immediate and deferred lists).
     */
    virtual ReturnValue remove_handler(const IFunction::ConstPtr &fn) const = 0;
    /**
     * @brief Returns true if the event has any handlers.
     */
    virtual bool has_handlers() const = 0;
};

/**
 * @brief Invokes an event with null safety.
 * @param event Event to invoke.
 * @param args Arguments for invocation.
 */
inline ReturnValue invoke_event(const IEvent::ConstPtr &event, FnArgs args)
{
    if (!event) return ReturnValue::INVALID_ARGUMENT;
    event->invoke(args);
    return ReturnValue::SUCCESS;
}

/** @brief Invokes an event with a single IAny argument. */
inline ReturnValue invoke_event(const IEvent::ConstPtr &event, const IAny *arg)
{
    FnArgs args{&arg, 1};
    if (!event) return ReturnValue::INVALID_ARGUMENT;
    event->invoke(args);
    return ReturnValue::SUCCESS;
}

} // namespace strata

#endif // INTF_EVENT_H
