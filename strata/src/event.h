#ifndef EVENT_H
#define EVENT_H

#include "strata_impl.h"
#include <common.h>
#include <ext/core_object.h>
#include <interface/intf_event.h>
#include <interface/intf_function.h>
#include <interface/types.h>

namespace strata {

class EventImpl final : public CoreObject<EventImpl, IEvent, IFunction>
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
    mutable std::vector<IFunction::ConstPtr> handlers_;
};

} // namespace strata

#endif // EVENT_H
