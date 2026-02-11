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
     */
    virtual ReturnValue add_handler(const IFunction::ConstPtr &fn) const = 0;
    /**
     * @brief Removes an event handler function.
     */
    virtual ReturnValue remove_handler(const IFunction::ConstPtr &fn) const = 0;
};

/**
 * @brief Invokes an event
 * @param event Event to invoke
 * @param args Arguments for invocation
 */
[[maybe_unused]] static ReturnValue invoke_event(const IEvent::ConstPtr &event, const IAny *args)
{
    return event ? event->get_invocable()->invoke(args) : ReturnValue::INVALID_ARGUMENT;
}

} // namespace strata

#endif // INTF_EVENT_H
