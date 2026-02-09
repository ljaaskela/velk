#ifndef PROPERTY_H
#define PROPERTY_H

#include "registry.h"
#include <common.h>
#include <ext/event.h>
#include <ext/object.h>
#include <interface/intf_property.h>
#include <interface/types.h>

class PropertyImpl final : public Object<PropertyImpl, IProperty, IPropertyInternal>
{
public:
    PropertyImpl() = default;

protected: // IProperty
    ReturnValue SetValue(const IAny &from) override;
    const IAny::ConstPtr GetValue() const override;
    IEvent::Ptr OnChanged() const override { return onChanged_; }

protected: // IPropertyInternal
    bool SetAny(const IAny::Ptr &value) override;
    IAny::Ptr GetAny() const override;

private:
    IAny::Ptr data_;
    LazyEvent onChanged_;
};

#endif // PROPERTY_H
