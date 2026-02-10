#ifndef EVENT_H
#define EVENT_H

#include "registry.h"
#include <common.h>
#include <ext/object.h>
#include <interface/intf_event.h>
#include <interface/intf_function.h>
#include <interface/types.h>

class EventImpl final : public Object<EventImpl, IEvent, IFunction>
{
public:
    EventImpl() = default;

protected: // IFunction
    ReturnValue Invoke(const IAny *args) const override;

protected: // IEvent
    const IFunction::ConstPtr GetInvocable() const override;
    ReturnValue AddHandler(const IFunction::ConstPtr &fn) const override;
    ReturnValue RemoveHandler(const IFunction::ConstPtr &fn) const override;

private:
    mutable vector<IFunction::ConstPtr> handlers_;
};

#endif // EVENT_H
