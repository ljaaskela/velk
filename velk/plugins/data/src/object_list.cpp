#include "object_list.h"

#include <velk/ext/any.h>

namespace velk::impl {

void ObjectList::fire(ItemChange change)
{
    auto evt = get_event("on_items_changed", Resolve::Existing);
    if (evt && evt->has_handlers()) {
        auto any = ext::create_any_ref(&change);
        const IAny* arg = any.get();
        evt->invoke(FnArgs{&arg, 1});
    }
}

size_t ObjectList::child_count(const IObject::Ptr& parent) const
{
    return parent ? 0 : size();
}

IObject::Ptr ObjectList::child_at(const IObject::Ptr& parent, size_t index) const
{
    return parent ? IObject::Ptr{} : at(index);
}

size_t ObjectList::size() const
{
    std::lock_guard lock(mutex_);
    return items_.size();
}

IObject::Ptr ObjectList::at(size_t index) const
{
    std::lock_guard lock(mutex_);
    return index < items_.size() ? items_[index] : IObject::Ptr{};
}

size_t ObjectList::index_of(const IObject::Ptr& item) const
{
    std::lock_guard lock(mutex_);
    for (size_t i = 0; i < items_.size(); ++i) {
        if (items_[i] == item) {
            return i;
        }
    }
    return items_.size();
}

ReturnValue ObjectList::add(const IObject::Ptr& item)
{
    if (!item) {
        return ReturnValue::InvalidArgument;
    }
    size_t index = 0;
    {
        std::lock_guard lock(mutex_);
        index = items_.size();
        items_.push_back(item);
    }
    fire({ItemChange::Type::Inserted, {}, index, 1});
    return ReturnValue::Success;
}

ReturnValue ObjectList::insert(size_t index, array_view<const IObject::Ptr> items)
{
    for (auto& item : items) {
        if (!item) {
            return ReturnValue::InvalidArgument;
        }
    }
    if (items.empty()) {
        return ReturnValue::NothingToDo;
    }
    {
        std::lock_guard lock(mutex_);
        if (index > items_.size()) {
            return ReturnValue::InvalidArgument;
        }
        vector<IObject::Ptr> next;
        next.reserve(items_.size() + items.size());
        for (size_t i = 0; i < index; ++i) {
            next.push_back(items_[i]);
        }
        for (auto& item : items) {
            next.push_back(item);
        }
        for (size_t i = index; i < items_.size(); ++i) {
            next.push_back(items_[i]);
        }
        items_ = std::move(next);
    }
    fire({ItemChange::Type::Inserted, {}, index, items.size()});
    return ReturnValue::Success;
}

ReturnValue ObjectList::remove(size_t index, size_t count)
{
    if (count == 0) {
        return ReturnValue::NothingToDo;
    }
    vector<IObject::Ptr> removed; // Released after the lock and the event.
    {
        std::lock_guard lock(mutex_);
        if (index >= items_.size() || count > items_.size() - index) {
            return ReturnValue::InvalidArgument;
        }
        vector<IObject::Ptr> next;
        next.reserve(items_.size() - count);
        for (size_t i = 0; i < items_.size(); ++i) {
            (i >= index && i < index + count ? removed : next).push_back(items_[i]);
        }
        items_ = std::move(next);
    }
    fire({ItemChange::Type::Removed, {}, index, count});
    return ReturnValue::Success;
}

ReturnValue ObjectList::move(size_t from, size_t to)
{
    IObject::Ptr item = at(from);
    if (!item || to >= size()) {
        return ReturnValue::InvalidArgument;
    }
    if (from == to) {
        return ReturnValue::NothingToDo;
    }
    remove(from, 1);
    const IObject::Ptr one[] = {item};
    return insert(to, one);
}

void ObjectList::clear()
{
    vector<IObject::Ptr> removed;
    {
        std::lock_guard lock(mutex_);
        removed = std::move(items_);
        items_ = {};
    }
    fire({ItemChange::Type::Reset});
}

} // namespace velk::impl
