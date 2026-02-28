#ifndef ARRAY_PROPERTY_H
#define ARRAY_PROPERTY_H

#include <velk/common.h>
#include <velk/ext/core_object.h>
#include <velk/ext/event.h>
#include <velk/interface/intf_array_any.h>
#include <velk/interface/intf_array_property.h>
#include <velk/interface/intf_property.h>
#include <velk/interface/types.h>

namespace velk {

/**
 * @brief Default IArrayProperty + IPropertyInternal implementation.
 *
 * Nearly identical to PropertyImpl but also implements IArrayProperty by
 * delegating element-level operations to IArrayAny on its backing data_.
 * The data_ is expected to be an ArrayAnyRef<T> (which implements IArrayAny).
 */
class ArrayPropertyImpl final
    : public ext::ObjectCore<ArrayPropertyImpl, IPropertyInternal, IArrayProperty>
{
public:
    VELK_CLASS_UID(ClassId::ArrayProperty);

    ArrayPropertyImpl() = default;

protected: // IProperty
    ReturnValue set_value(const IAny& from, InvokeType type = Immediate) override;
    const IAny::ConstPtr get_value() const override;
    IEvent::Ptr on_changed() const override { return onChanged_; }

protected: // IPropertyInternal
    bool set_any(const IAny::Ptr& value) override;
    IAny::ConstPtr get_any() const override;
    ReturnValue set_data(const void* data, size_t size, Uid type, InvokeType invokeType = Immediate) override;
    ReturnValue set_value_silent(const IAny& from) override;

protected: // IArrayProperty
    size_t array_size() const override;
    ReturnValue get_at(size_t index, IAny& out) const override;
    ReturnValue set_at(size_t index, const IAny& value) override;
    ReturnValue push_back(const IAny& value) override;
    ReturnValue erase_at(size_t index) override;
    void clear_array() override;

private:
    IArrayAny* get_array_any() const;

    IAny::Ptr data_;
    ext::LazyEvent onChanged_;
};

} // namespace velk

#endif // ARRAY_PROPERTY_H
