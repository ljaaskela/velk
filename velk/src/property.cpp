#include "property.h"

#include <velk/api/velk.h>
#include <velk/interface/intf_any_extension.h>
#include <velk/interface/intf_external_any.h>
#include <velk/interface/types.h>

namespace velk {

ReturnValue PropertyImpl::set_value(const IAny& from, InvokeType type)
{
    if (get_object_data().flags & ObjectFlags::ReadOnly) {
        return ReturnValue::ReadOnly;
    }
    if (!data_) {
        return ReturnValue::Fail;
    }
    if (type == Deferred) {
        // Create a clone with value "from" and store it in the deferred callback
        auto clone = data_->clone();
        if (clone && clone->copy_from(from) == ReturnValue::Success) {
            instance().queue_deferred_property({get_self<IPropertyInternal>(), std::move(clone)});
            return ReturnValue::Success;
        }
        return ReturnValue::Fail;
    }
    // type == Immediate, just copy the value
    auto ret = data_->copy_from(from);
    if (ret == ReturnValue::Success && !external_) {
        invoke_event(on_changed(), data_.get());
    }
    return ret;
}
const IAny::ConstPtr PropertyImpl::get_value() const
{
    return data_;
}
bool PropertyImpl::set_any(const IAny::Ptr& value, IAny::Ptr* previous)
{
    if (previous) {
        *previous = {};
    }
    if (data_ && value) {
        // Disconnect old external wiring if present
        if (external_) {
            if (auto ext = interface_cast<IExternalAny>(data_)) {
                ext->on_data_changed()->remove_handler(on_changed());
            }
        }
        if (previous) {
            *previous = data_;
        }
    }
    data_ = value;
    auto external = interface_cast<IExternalAny>(data_);
    external_ = external != nullptr;
    if (external) {
        // External any fires on_data_changed when its data changes, so connect it to
        // our on_changed. PropertyImpl skips its own explicit fire for external anys.
        external->on_data_changed()->add_handler(on_changed());
    }
    return succeeded(invoke_event(on_changed(), data_.get()));
}
IAny::ConstPtr PropertyImpl::get_any() const
{
    return data_;
}
ReturnValue PropertyImpl::set_value_silent(const IAny& from)
{
    if (get_object_data().flags & ObjectFlags::ReadOnly) {
        return ReturnValue::ReadOnly;
    }
    if (!data_) {
        return ReturnValue::Fail;
    }
    auto ret = data_->copy_from(from);
    // External anys fire on_data_changed -> on_changed automatically during copy_from,
    // so return NothingToDo to prevent the caller from firing on_changed again.
    if (ret == ReturnValue::Success && external_) {
        return ReturnValue::NothingToDo;
    }
    return ret;
}

ReturnValue PropertyImpl::set_data(const void* data, size_t size, Uid type, InvokeType invokeType)
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
    if (ret == ReturnValue::Success && !external_) {
        invoke_event(on_changed(), data_.get());
    }
    return ret;
}

bool PropertyImpl::install_extension(const IAnyExtension::Ptr& extension)
{
    if (!extension) {
        return false;
    }
    extension->set_inner(data_, IInterface::WeakPtr(get_self<IInterface>()));
    set_any(interface_pointer_cast<IAny>(extension));
    return true;
}

bool PropertyImpl::remove_extension(const IAnyExtension::Ptr& extension)
{
    if (!extension || !data_) {
        return false;
    }

    auto ext_as_any = interface_pointer_cast<IAny>(extension);

    IInterface& self = static_cast<IPropertyInternal&>(*this);

    // Case 1: extension is at the head of the chain
    if (data_ == ext_as_any) {
        auto inner = extension->take_inner(self);
        set_any(inner);
        return true;
    }

    // Case 2: extension is deeper in the chain; walk to find its predecessor.
    // Use take/restore to get non-const access to each link.
    auto weakSelf = IInterface::WeakPtr(get_self<IInterface>());
    auto* prev = interface_cast<IAnyExtension>(data_);
    while (prev) {
        auto inner = prev->take_inner(self);
        if (inner == ext_as_any) {
            prev->set_inner(extension->take_inner(self), weakSelf);
            return true;
        }
        auto* next = interface_cast<IAnyExtension>(inner);
        prev->set_inner(std::move(inner), weakSelf);
        prev = next;
    }
    return false;
}

} // namespace velk
