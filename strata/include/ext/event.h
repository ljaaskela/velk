#ifndef EXT_EVENT_H
#define EXT_EVENT_H

#include <api/strata.h>
#include <interface/intf_event.h>
#include <interface/types.h>

namespace strata {

/**
 * @brief Helper that lazily creates an IEvent on first access.
 *
 * Implicitly converts to IEvent::Ptr, creating the event via Strata
 * on the first conversion. Subsequent accesses return the cached instance.
 */
class LazyEvent {
    mutable IEvent::Ptr event_;
public:
    /** @brief Returns the event, creating it on first access. */
    operator IEvent::Ptr() const {
        if (!event_) {
            event_ = instance().create<IEvent>(ClassId::Event);
        }
        return event_;
    }
};

} // namespace strata

#endif // EXT_EVENT_H
