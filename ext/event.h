#ifndef EXT_EVENT_H
#define EXT_EVENT_H

#include <common.h>
#include <interface/intf_event.h>
#include <interface/intf_registry.h>

#define ACCESS_EVENT(Name) event##Name_
#define IMPLEMENT_EVENT(Name) \
protected: \
    IEvent::Ptr Name() const override { return event##Name_; } \
\
private: \
    IEvent::Ptr event##Name_ = GetRegistry().Create<IEvent>(ClassId::Event);

#endif // EXT_EVENT_H
