#include "container.h"

namespace velk {

ReturnValue ContainerImpl::add(const IObject::Ptr& child)
{
    if (!child) {
        return ReturnValue::InvalidArgument;
    }
    children_.push_back(child);
    return ReturnValue::Success;
}

ReturnValue ContainerImpl::remove(const IObject::Ptr& child)
{
    if (!child) {
        return ReturnValue::InvalidArgument;
    }
    for (auto it = children_.begin(); it != children_.end(); ++it) {
        if (it->get() == child.get()) {
            children_.erase(it);
            return ReturnValue::Success;
        }
    }
    return ReturnValue::NothingToDo;
}

ReturnValue ContainerImpl::insert(size_t index, const IObject::Ptr& child)
{
    if (!child) {
        return ReturnValue::InvalidArgument;
    }
    if (index > children_.size()) {
        return ReturnValue::InvalidArgument;
    }
    children_.insert(children_.begin() + static_cast<ptrdiff_t>(index), child);
    return ReturnValue::Success;
}

ReturnValue ContainerImpl::replace(size_t index, const IObject::Ptr& child)
{
    if (!child) {
        return ReturnValue::InvalidArgument;
    }
    if (index >= children_.size()) {
        return ReturnValue::InvalidArgument;
    }
    children_[index] = child;
    return ReturnValue::Success;
}

IObject::Ptr ContainerImpl::get_at(size_t index) const
{
    if (index >= children_.size()) {
        return {};
    }
    return children_[index];
}

vector<IObject::Ptr> ContainerImpl::get_all() const
{
    vector<IObject::Ptr> result;
    result.reserve(children_.size());
    for (auto& child : children_) {
        result.push_back(child);
    }
    return result;
}

size_t ContainerImpl::size() const
{
    return children_.size();
}

void ContainerImpl::clear()
{
    children_.clear();
}

} // namespace velk
