#include "hierarchy.h"

namespace velk {

ReturnValue HierarchyImpl::set_root(const IObject::Ptr& root)
{
    if (!root) {
        return ReturnValue::InvalidArgument;
    }
    clear();
    root_ = root;
    entries_[root.get()] = {root, nullptr, {}};
    return ReturnValue::Success;
}

ReturnValue HierarchyImpl::add(const IObject::Ptr& parent, const IObject::Ptr& child)
{
    if (!parent || !child) {
        return ReturnValue::InvalidArgument;
    }
    auto pit = entries_.find(parent.get());
    if (pit == entries_.end()) {
        return ReturnValue::InvalidArgument;
    }
    if (entries_.find(child.get()) != entries_.end()) {
        return ReturnValue::InvalidArgument;
    }
    pit->second.children.push_back(child);
    entries_[child.get()] = {child, parent.get(), {}};
    return ReturnValue::Success;
}

ReturnValue HierarchyImpl::insert(const IObject::Ptr& parent, size_t index, const IObject::Ptr& child)
{
    if (!parent || !child) {
        return ReturnValue::InvalidArgument;
    }
    auto pit = entries_.find(parent.get());
    if (pit == entries_.end()) {
        return ReturnValue::InvalidArgument;
    }
    if (entries_.find(child.get()) != entries_.end()) {
        return ReturnValue::InvalidArgument;
    }
    auto& children = pit->second.children;
    if (index > children.size()) {
        return ReturnValue::InvalidArgument;
    }
    children.insert(children.begin() + static_cast<ptrdiff_t>(index), child);
    entries_[child.get()] = {child, parent.get(), {}};
    return ReturnValue::Success;
}

ReturnValue HierarchyImpl::remove(const IObject::Ptr& object)
{
    if (!object) {
        return ReturnValue::InvalidArgument;
    }
    auto it = entries_.find(object.get());
    if (it == entries_.end()) {
        return ReturnValue::NothingToDo;
    }
    // If removing root, clear everything
    if (object.get() == root_.get()) {
        clear();
        return ReturnValue::Success;
    }
    // Remove from parent's child list
    auto* parentPtr = it->second.parent;
    if (parentPtr) {
        auto pit = entries_.find(parentPtr);
        if (pit != entries_.end()) {
            auto& siblings = pit->second.children;
            for (auto sit = siblings.begin(); sit != siblings.end(); ++sit) {
                if (sit->get() == object.get()) {
                    siblings.erase(sit);
                    break;
                }
            }
        }
    }
    // Remove object and all descendants recursively
    remove_recursive(object.get());
    return ReturnValue::Success;
}

ReturnValue HierarchyImpl::replace(const IObject::Ptr& old_child, const IObject::Ptr& new_child)
{
    if (!old_child || !new_child) {
        return ReturnValue::InvalidArgument;
    }
    auto it = entries_.find(old_child.get());
    if (it == entries_.end()) {
        return ReturnValue::InvalidArgument;
    }
    if (entries_.find(new_child.get()) != entries_.end()) {
        return ReturnValue::InvalidArgument;
    }
    auto* parentPtr = it->second.parent;
    auto old_children = std::move(it->second.children);

    // Remove old entry
    entries_.erase(it);

    // Update parent's child list to point to new child
    if (parentPtr) {
        auto pit = entries_.find(parentPtr);
        if (pit != entries_.end()) {
            for (auto& c : pit->second.children) {
                if (c.get() == old_child.get()) {
                    c = new_child;
                    break;
                }
            }
        }
    } else {
        // Replacing root
        root_ = new_child;
    }

    // Insert new entry with old children and same parent
    entries_[new_child.get()] = {new_child, parentPtr, std::move(old_children)};

    // Update children's parent pointers to new_child
    auto& newEntry = entries_[new_child.get()];
    for (auto& c : newEntry.children) {
        auto cit = entries_.find(c.get());
        if (cit != entries_.end()) {
            cit->second.parent = new_child.get();
        }
    }

    return ReturnValue::Success;
}

void HierarchyImpl::clear()
{
    root_ = {};
    entries_.clear();
}

IObject::Ptr HierarchyImpl::root() const
{
    return root_;
}

HierarchyNode HierarchyImpl::node_of(const IObject::Ptr& object) const
{
    if (!object) {
        return {};
    }
    auto it = entries_.find(object.get());
    if (it == entries_.end()) {
        return {};
    }
    return {it->second.object, get_self<IHierarchy>()};
}

IObject::Ptr HierarchyImpl::parent_of(const IObject::Ptr& object) const
{
    if (!object) {
        return {};
    }
    auto it = entries_.find(object.get());
    if (it == entries_.end() || !it->second.parent) {
        return {};
    }
    auto pit = entries_.find(it->second.parent);
    return pit != entries_.end() ? pit->second.object : IObject::Ptr{};
}

vector<IObject::Ptr> HierarchyImpl::children_of(const IObject::Ptr& object) const
{
    if (!object) {
        return {};
    }
    auto it = entries_.find(object.get());
    if (it == entries_.end()) {
        return {};
    }
    vector<IObject::Ptr> result;
    result.reserve(it->second.children.size());
    for (auto& c : it->second.children) {
        result.push_back(c);
    }
    return result;
}

IObject::Ptr HierarchyImpl::child_at(const IObject::Ptr& object, size_t index) const
{
    if (!object) {
        return {};
    }
    auto it = entries_.find(object.get());
    if (it == entries_.end() || index >= it->second.children.size()) {
        return {};
    }
    return it->second.children[index];
}

size_t HierarchyImpl::child_count(const IObject::Ptr& object) const
{
    if (!object) {
        return 0;
    }
    auto it = entries_.find(object.get());
    return it != entries_.end() ? it->second.children.size() : 0;
}

bool HierarchyImpl::contains(const IObject::Ptr& object) const
{
    return object && entries_.find(object.get()) != entries_.end();
}

size_t HierarchyImpl::size() const
{
    return entries_.size();
}

void HierarchyImpl::remove_recursive(IObject* obj)
{
    auto it = entries_.find(obj);
    if (it == entries_.end()) {
        return;
    }
    // Copy children before erasing (iterator invalidation)
    auto children = std::move(it->second.children);
    entries_.erase(it);
    for (auto& c : children) {
        remove_recursive(c.get());
    }
}

} // namespace velk
