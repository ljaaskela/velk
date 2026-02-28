#include "array_property.h"

#include <velk/api/velk.h>
#include <velk/interface/types.h>

namespace velk {

// IProperty

ReturnValue ArrayPropertyImpl::set_value(const IAny& from, InvokeType type)
{
    if (get_object_data().flags & ObjectFlags::ReadOnly) {
        return ReturnValue::ReadOnly;
    }
    if (!data_) {
        return ReturnValue::Fail;
    }
    if (type == Deferred) {
        auto clone = data_->clone();
        if (clone && clone->copy_from(from) == ReturnValue::Success) {
            instance().queue_deferred_property({get_self<IPropertyInternal>(), std::move(clone)});
            return ReturnValue::Success;
        }
        return ReturnValue::Fail;
    }
    auto ret = data_->copy_from(from);
    if (ret == ReturnValue::Success) {
        invoke_event(on_changed(), data_.get());
    }
    return ret;
}

const IAny::ConstPtr ArrayPropertyImpl::get_value() const
{
    return data_;
}

// IPropertyInternal

bool ArrayPropertyImpl::set_any(const IAny::Ptr& value)
{
    if (data_ && value) {
        return false;
    }
    data_ = value;
    return succeeded(invoke_event(on_changed(), data_.get()));
}

IAny::ConstPtr ArrayPropertyImpl::get_any() const
{
    return data_;
}

ReturnValue ArrayPropertyImpl::set_data(const void* data, size_t size, Uid type, InvokeType invokeType)
{
    if (get_object_data().flags & ObjectFlags::ReadOnly) {
        return ReturnValue::ReadOnly;
    }
    if (!data_) {
        return ReturnValue::Fail;
    }
    if (invokeType == Deferred) {
        auto clone = data_->clone();
        if (clone && clone->set_data(data, size, type) == ReturnValue::Success) {
            instance().queue_deferred_property({get_self<IPropertyInternal>(), std::move(clone)});
            return ReturnValue::Success;
        }
        return ReturnValue::Fail;
    }
    auto ret = data_->set_data(data, size, type);
    if (ret == ReturnValue::Success) {
        invoke_event(on_changed(), data_.get());
    }
    return ret;
}

ReturnValue ArrayPropertyImpl::set_value_silent(const IAny& from)
{
    if (get_object_data().flags & ObjectFlags::ReadOnly) {
        return ReturnValue::ReadOnly;
    }
    if (!data_) {
        return ReturnValue::Fail;
    }
    return data_->copy_from(from);
}

// IArrayProperty (delegates to IArrayAny on data_)

IArrayAny* ArrayPropertyImpl::get_array_any() const
{
    return data_ ? interface_cast<IArrayAny>(data_) : nullptr;
}

size_t ArrayPropertyImpl::array_size() const
{
    auto* aa = get_array_any();
    return aa ? aa->array_size() : 0;
}

ReturnValue ArrayPropertyImpl::get_at(size_t index, IAny& out) const
{
    auto* aa = get_array_any();
    return aa ? aa->get_at(index, out) : ReturnValue::Fail;
}

ReturnValue ArrayPropertyImpl::set_at(size_t index, const IAny& value)
{
    if (get_object_data().flags & ObjectFlags::ReadOnly) {
        return ReturnValue::ReadOnly;
    }
    auto* aa = get_array_any();
    if (!aa) {
        return ReturnValue::Fail;
    }
    auto ret = aa->set_at(index, value);
    if (succeeded(ret)) {
        invoke_event(on_changed(), data_.get());
    }
    return ret;
}

ReturnValue ArrayPropertyImpl::push_back(const IAny& value)
{
    if (get_object_data().flags & ObjectFlags::ReadOnly) {
        return ReturnValue::ReadOnly;
    }
    auto* aa = get_array_any();
    if (!aa) {
        return ReturnValue::Fail;
    }
    auto ret = aa->push_back(value);
    if (succeeded(ret)) {
        invoke_event(on_changed(), data_.get());
    }
    return ret;
}

ReturnValue ArrayPropertyImpl::erase_at(size_t index)
{
    if (get_object_data().flags & ObjectFlags::ReadOnly) {
        return ReturnValue::ReadOnly;
    }
    auto* aa = get_array_any();
    if (!aa) {
        return ReturnValue::Fail;
    }
    auto ret = aa->erase_at(index);
    if (succeeded(ret)) {
        invoke_event(on_changed(), data_.get());
    }
    return ret;
}

void ArrayPropertyImpl::clear_array()
{
    if (get_object_data().flags & ObjectFlags::ReadOnly) {
        return;
    }
    auto* aa = get_array_any();
    if (!aa) {
        return;
    }
    aa->clear_array();
    invoke_event(on_changed(), data_.get());
}

} // namespace velk
