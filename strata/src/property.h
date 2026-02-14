#ifndef PROPERTY_H
#define PROPERTY_H

#include "strata_impl.h"
#include <common.h>
#include <ext/event.h>
#include <ext/core_object.h>
#include <interface/intf_property.h>
#include <interface/types.h>

namespace strata {

class PropertyImpl final : public ext::ObjectCore<PropertyImpl, IProperty, IPropertyInternal>
{
public:
    PropertyImpl() = default;

protected: // IProperty
    ReturnValue set_value(const IAny &from) override;
    const IAny::ConstPtr get_value() const override;
    IEvent::Ptr on_changed() const override { return onChanged_; }

protected: // IPropertyInternal
    bool set_any(const IAny::Ptr &value) override;
    IAny::ConstPtr get_any() const override;
    ReturnValue set_data(const void *data, size_t size, Uid type) override;

private:
    IAny::Ptr data_;
    ext::LazyEvent onChanged_;
    bool external_{};  ///< True if data_ implements IExternalAny (on_data_changed fires on_changed automatically).
};

} // namespace strata

#endif // PROPERTY_H
