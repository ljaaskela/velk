#include "event.h"

ReturnValue EventImpl::Invoke(const IAny *args) const
{
    for (const auto &h : handlers_) {
        h->Invoke(args);
    }
    return ReturnValue::SUCCESS;
}

const IFunction::ConstPtr EventImpl::GetInvocable() const
{
    return GetSelf<IFunction>();
}

ReturnValue EventImpl::AddHandler(const IFunction::ConstPtr &fn) const
{
    if (!fn) {
        return ReturnValue::INVALID_ARGUMENT;
    }
    for (const auto &h : handlers_) {
        if (h == fn) {
            return ReturnValue::NOTHING_TO_DO;
        }
    }
    handlers_.push_back(fn);
    return ReturnValue::SUCCESS;
}

ReturnValue EventImpl::RemoveHandler(const IFunction::ConstPtr &fn) const
{
    auto pos = 0;
    for (const auto &h : handlers_) {
        if (h == fn) {
            handlers_.erase(handlers_.begin() + pos);
            return ReturnValue::SUCCESS;
        }
        pos++;
    }
    return ReturnValue::NOTHING_TO_DO;
}
