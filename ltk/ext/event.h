#ifndef EXT_EVENT_H
#define EXT_EVENT_H

#include <api/ltk.h>
#include <interface/intf_event.h>
#include <interface/types.h>

class LazyEvent {
    mutable IEvent::Ptr event_;
public:
    operator IEvent::Ptr() const {
        if (!event_) {
            event_ = GetRegistry().Create<IEvent>(ClassId::Event);
        }
        return event_;
    }
};

#endif // EXT_EVENT_H
