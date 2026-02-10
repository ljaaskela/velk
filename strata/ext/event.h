#ifndef EXT_EVENT_H
#define EXT_EVENT_H

#include <api/strata.h>
#include <interface/intf_event.h>
#include <interface/types.h>

/**
 * @brief Helper that lazily creates an IEvent on first access.
 *
 * Implicitly converts to IEvent::Ptr, creating the event via the registry
 * on the first conversion. Subsequent accesses return the cached instance.
 */
class LazyEvent {
    mutable IEvent::Ptr event_;
public:
    /** @brief Returns the event, creating it on first access. */
    operator IEvent::Ptr() const {
        if (!event_) {
            event_ = GetRegistry().Create<IEvent>(ClassId::Event);
        }
        return event_;
    }
};

#endif // EXT_EVENT_H
