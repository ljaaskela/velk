#include "property.h"
#include <interface/intf_external_any.h>

namespace strata {

ReturnValue PropertyImpl::set_value(const IAny &from)
{
    auto ret = ReturnValue::FAIL;
    if (data_) {
        ret = data_->copy_from(from);
        if (ret == ReturnValue::SUCCESS) {
            return invoke_event(on_changed(), data_.get());
        }
    }
    return ret;
}
const IAny::ConstPtr PropertyImpl::get_value() const
{
    return data_;
}
bool PropertyImpl::set_any(const IAny::Ptr &value)
{
    if (data_ && value) {
        std::cerr << "Property data already set" << std::endl;
        return false;
    }
    data_ = value;
    if (auto external = interface_cast<IExternalAny>(data_)) {
        // If the any type can be edited externally, connect any's on_data_changed to
        // our on_changed
        external->on_data_changed()->add_handler(on_changed());
    }
    return succeeded(invoke_event(on_changed(), data_.get()));
}
IAny::Ptr PropertyImpl::get_any() const
{
    return data_;
}

} // namespace strata
