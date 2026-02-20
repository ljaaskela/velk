#include "property.h"
#include <velk/interface/intf_external_any.h>
#include <velk/interface/types.h>

namespace velk {

ReturnValue PropertyImpl::set_value(const IAny &from)
{
    if (get_object_data().flags & ObjectFlags::ReadOnly) {
        return ReturnValue::READ_ONLY;
    }
    if (data_) {
        auto ret = data_->copy_from(from);
        if (ret == ReturnValue::SUCCESS && !external_) {
            invoke_event(on_changed(), data_.get());
        }
        return ret;
    }
    return ReturnValue::FAIL;
}
const IAny::ConstPtr PropertyImpl::get_value() const
{
    return data_;
}
bool PropertyImpl::set_any(const IAny::Ptr &value)
{
    if (data_ && value) {
        return false;
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
void PropertyImpl::set_flags(int32_t flags)
{
    get_object_data().flags = flags;
}

ReturnValue PropertyImpl::set_data(const void *data, size_t size, Uid type)
{
    if (get_object_data().flags & ObjectFlags::ReadOnly) {
        return ReturnValue::READ_ONLY;
    }
    auto ret = ReturnValue::FAIL;
    if (data_) {
        ret = data_->set_data(data, size, type);
        if (ret == ReturnValue::SUCCESS && !external_) {
            // Ignore return value since data was successfully set
            invoke_event(on_changed(), data_.get());
        }
    }
    return ret;
}

} // namespace velk
