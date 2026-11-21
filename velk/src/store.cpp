#include "store.h"

#include <mutex>

namespace velk::impl {

IObject::Ptr Store::find(string_view id) const
{
    std::shared_lock lock(mutex_);
    for (const auto& entry : entries_) {
        if (entry.id == id) {
            return entry.object;
        }
    }
    return {};
}

size_t Store::object_count() const
{
    std::shared_lock lock(mutex_);
    return entries_.size();
}

IObject::Ptr Store::object_at(size_t index) const
{
    std::shared_lock lock(mutex_);
    return index < entries_.size() ? entries_[index].object : nullptr;
}

string Store::id_at(size_t index) const
{
    std::shared_lock lock(mutex_);
    return index < entries_.size() ? entries_[index].id : string();
}

ReturnValue Store::add(string_view id, const IObject::Ptr& object)
{
    if (id.empty() || !object) {
        return ReturnValue::InvalidArgument;
    }
    std::unique_lock lock(mutex_);
    for (const auto& entry : entries_) {
        if (entry.id == id) {
            return ReturnValue::Fail;
        }
    }
    entries_.push_back({id, object});
    return ReturnValue::Success;
}

} // namespace velk::impl
