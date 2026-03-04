#ifndef HIERARCHY_H
#define HIERARCHY_H

#include <velk/ext/core_object.h>
#include <velk/interface/intf_hierarchy.h>

#include <unordered_map>
#include <vector>

namespace velk {

/**
 * @brief Default implementation of IHierarchy backed by an unordered_map.
 */
class HierarchyImpl final : public ext::ObjectCore<HierarchyImpl, IHierarchy>
{
public:
    VELK_CLASS_UID(ClassId::Hierarchy);

    ReturnValue set_root(const IObject::Ptr& root) override;
    ReturnValue add(const IObject::Ptr& parent, const IObject::Ptr& child) override;
    ReturnValue insert(const IObject::Ptr& parent, size_t index, const IObject::Ptr& child) override;
    ReturnValue remove(const IObject::Ptr& object) override;
    ReturnValue replace(const IObject::Ptr& old_child, const IObject::Ptr& new_child) override;
    void clear() override;

    IObject::Ptr root() const override;
    HierarchyNode node_of(const IObject::Ptr& object) const override;
    IObject::Ptr parent_of(const IObject::Ptr& object) const override;
    vector<IObject::Ptr> children_of(const IObject::Ptr& object) const override;
    IObject::Ptr child_at(const IObject::Ptr& object, size_t index) const override;
    size_t child_count(const IObject::Ptr& object) const override;
    bool contains(const IObject::Ptr& object) const override;
    size_t size() const override;

private:
    struct Entry
    {
        IObject::Ptr object;
        IObject* parent = nullptr;
        std::vector<IObject::Ptr> children;
    };

    void remove_recursive(IObject* obj);

    IObject::Ptr root_;
    std::unordered_map<IObject*, Entry> entries_;
};

} // namespace velk

#endif // HIERARCHY_H
