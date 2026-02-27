#ifndef PROPERTY_H
#define PROPERTY_H

#include <velk/common.h>
#include <velk/ext/core_object.h>
#include <velk/ext/event.h>
#include <velk/interface/intf_property.h>
#include <velk/interface/types.h>

namespace velk {

/**
 * @brief Default IProperty/IPropertyInternal implementation.
 *
 * Stores a type-erased value via an IAny pointer. Fires on_changed when
 * set_value/set_data modifies the value. Supports read-only mode via
 * ObjectFlags::ReadOnly. When the backing IAny implements IExternalAny,
 * automatically relays its on_data_changed event to the property's on_changed.
 */
class PropertyImpl final : public ext::ObjectCore<PropertyImpl, IPropertyInternal>
{
public:
    VELK_CLASS_UID(ClassId::Property);

    PropertyImpl() = default;

protected: // IProperty
    ReturnValue set_value(const IAny& from, InvokeType type = Immediate) override;
    const IAny::ConstPtr get_value() const override;
    IEvent::Ptr on_changed() const override { return onChanged_; }

protected: // IPropertyInternal
    bool set_any(const IAny::Ptr& value) override;
    IAny::ConstPtr get_any() const override;
    ReturnValue set_data(const void* data, size_t size, Uid type, InvokeType invokeType = Immediate) override;
    ReturnValue set_value_silent(const IAny& from) override;

private:
    IAny::Ptr data_;
    ext::LazyEvent onChanged_;
    bool external_{}; ///< True if data_ implements IExternalAny (on_data_changed fires on_changed
                      ///< automatically).
};

} // namespace velk

#endif // PROPERTY_H
