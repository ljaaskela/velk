#include "hierarchy_source.h"

#include <velk/api/state.h>
#include <velk/ext/any.h>

namespace velk::impl {

IHierarchy::Ptr HierarchySource::hierarchy() const
{
    auto state = read_state<IHierarchySource>(this);
    return state ? state->hierarchy.get<IHierarchy>() : nullptr;
}

size_t HierarchySource::child_count(const IObject::Ptr& parent) const
{
    auto h = hierarchy();
    if (!h) {
        return 0;
    }
    if (!parent) {
        return h->root() ? 1 : 0;
    }
    return h->child_count(parent);
}

IObject::Ptr HierarchySource::child_at(const IObject::Ptr& parent, size_t index) const
{
    auto h = hierarchy();
    if (!h) {
        return {};
    }
    if (!parent) {
        return index == 0 ? h->root() : IObject::Ptr{};
    }
    return h->child_at(parent, index);
}

bool HierarchySource::has_children(const IObject::Ptr& item) const
{
    auto h = hierarchy();
    return h && item && h->child_count(item) > 0;
}

void HierarchySource::on_state_changed(string_view name, IMetadata&, Uid interfaceId)
{
    constexpr string_view names[] = {"hierarchy"};
    if (has_state_changed<IHierarchySource>(interfaceId, name, names)) {
        auto h = hierarchy();
        if (h != watched_.lock()) {
            watch();
            fire({ItemChange::Type::Reset});
        }
    }
}

void HierarchySource::watch()
{
    changing_ = {};
    changed_ = {};
    auto h = hierarchy();
    watched_ = h;
    if (h) {
        changing_ = ScopedHandler(h->on_changing(), [this](const HierarchyChange& c) { on_changing(c); });
        changed_ = ScopedHandler(h->on_changed(), [this](const HierarchyChange& c) { on_changed(c); });
    }
}

void HierarchySource::fire(ItemChange change)
{
    auto evt = get_event("on_items_changed", Resolve::Existing);
    if (evt && evt->has_handlers()) {
        auto any = ext::create_any_ref(&change);
        const IAny* arg = any.get();
        evt->invoke(FnArgs{&arg, 1});
    }
}

size_t HierarchySource::index_in(IHierarchy& h, const IObject::Ptr& parent, const IObject::Ptr& child)
{
    const size_t n = h.child_count(parent);
    for (size_t i = 0; i < n; ++i) {
        if (h.child_at(parent, i) == child) {
            return i;
        }
    }
    return n;
}

void HierarchySource::on_changing(const HierarchyChange& change)
{
    // A removal's index is gone once it happened; take it now.
    auto h = hierarchy();
    if (!h) {
        return;
    }
    if (change.type == HierarchyChange::Type::Remove && change.parent) {
        pending_index_ = index_in(*h, change.parent, change.child);
    } else if (change.type == HierarchyChange::Type::Replace && change.parent) {
        pending_index_ = index_in(*h, change.parent, change.old_child);
    }
}

void HierarchySource::on_changed(const HierarchyChange& change)
{
    auto h = hierarchy();
    if (!h) {
        return;
    }
    using T = HierarchyChange::Type;
    switch (change.type) {
    case T::Add:
        fire({ItemChange::Type::Inserted, change.parent, h->child_count(change.parent) - 1, 1});
        break;
    case T::Insert:
        fire({ItemChange::Type::Inserted, change.parent, change.index, 1});
        break;
    case T::Remove:
        // Removing the root empties the hierarchy.
        fire(change.parent ? ItemChange{ItemChange::Type::Removed, change.parent, pending_index_, 1}
                           : ItemChange{ItemChange::Type::Reset});
        break;
    case T::Replace:
        if (change.parent) {
            fire({ItemChange::Type::Removed, change.parent, pending_index_, 1});
            fire({ItemChange::Type::Inserted, change.parent, pending_index_, 1});
        } else {
            fire({ItemChange::Type::Reset});
        }
        break;
    case T::SetRoot:
    case T::Clear:
        fire({ItemChange::Type::Reset});
        break;
    }
}

} // namespace velk::impl
