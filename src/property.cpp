#include "property.h"
#include <interface/intf_external_any.h>

ReturnValue PropertyImpl::SetValue(const IAny &from)
{
    auto ret = ReturnValue::FAIL;
    if (data_) {
        ret = data_->CopyFrom(from);
        if (ret == ReturnValue::SUCCESS) {
            return InvokeEvent(ACCESS_EVENT(OnChanged), data_.get());
        }
    }
    return ret;
}
const IAny::ConstPtr PropertyImpl::GetValue() const
{
    return data_;
}
bool PropertyImpl::SetAny(const IAny::Ptr &value)
{
    if (data_ && value) {
        std::cerr << "Property data already set" << std::endl;
        return false;
    }
    data_ = value;
    if (auto external = data_->GetInterface<IExternalAny>()) {
        // If the any type can be edited externally, connect any's OnDataChanged to
        // our OnChanged
        external->OnDataChanged()->AddHandler(ACCESS_EVENT(OnChanged)->GetInvocable());
    }
    return Succeeded(InvokeEvent(ACCESS_EVENT(OnChanged), data_.get()));
}
IAny::Ptr PropertyImpl::GetAny() const
{
    return data_;
}
