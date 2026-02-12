#ifndef INTF_EVENT_H
#define INTF_EVENT_H

#include <common.h>
#include <interface/intf_function.h>
#include <interface/intf_interface.h>

namespace strata {

/**
 * @brief Interface for an event that supports multiple handler functions.
 *
 * Handlers can be added/removed, and the event can be invoked through its invocable.
 * Each handler is registered as either Immediate (called synchronously when the event
 * fires) or Deferred (queued for the next update() call).
 */
class IEvent : public Interface<IEvent>
{
public:
    /**
     * @brief Returns an invocable which can be used to invoke the event.
     */
    virtual const IFunction::ConstPtr get_invocable() const = 0;
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
 * @brief Invokes an event
 * @param event Event to invoke
 * @param args Arguments for invocation
 */
[[maybe_unused]] static ReturnValue invoke_event(const IEvent::Ptr &event, const IAny *args)
{
    return event ? event->get_invocable()->invoke(args) : ReturnValue::INVALID_ARGUMENT;
}

/** @copydoc invoke_event */
[[maybe_unused]] static ReturnValue invoke_event(const IEvent::ConstPtr &event, const IAny *args)
{
    return event ? event->get_invocable()->invoke(args) : ReturnValue::INVALID_ARGUMENT;
}

} // namespace strata

#endif // INTF_EVENT_H
