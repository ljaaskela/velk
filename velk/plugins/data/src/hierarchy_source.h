#ifndef VELK_DATA_HIERARCHY_SOURCE_H
#define VELK_DATA_HIERARCHY_SOURCE_H

#include <velk/api/event.h>
#include <velk/ext/object.h>
#include <velk/interface/intf_hierarchy.h>
#include <velk/interface/intf_metadata_observer.h>
#include <velk/plugins/data/interface/intf_hierarchy_source.h>
#include <velk/plugins/data/plugin.h>

namespace velk::impl {

/** @brief See IHierarchySource. */
class HierarchySource final : public ext::Object<HierarchySource, IItemSource, IHierarchySource, IMetadataObserver>
{
public:
    VELK_CLASS_UID(ClassId::HierarchySource, "HierarchySource");

    // IItemSource
    size_t child_count(const IObject::Ptr& parent) const override;
    IObject::Ptr child_at(const IObject::Ptr& parent, size_t index) const override;
    bool has_children(const IObject::Ptr& item) const override;

    // IMetadataObserver
    void on_state_changed(string_view name, IMetadata& owner, Uid interfaceId) override;

private:
    IHierarchy::Ptr hierarchy() const;
    void watch();
    void fire(ItemChange change);
    /** @brief The index of @p child under @p parent, or the child count when absent. */
    static size_t index_in(IHierarchy& h, const IObject::Ptr& parent, const IObject::Ptr& child);

    void on_changing(const HierarchyChange& change);
    void on_changed(const HierarchyChange& change);

    weak_ptr<IHierarchy> watched_;
    ScopedHandler changing_;
    ScopedHandler changed_;
    size_t pending_index_ = 0; ///< Index of the object a Remove or Replace is about to take out.
};

} // namespace velk::impl

#endif // VELK_DATA_HIERARCHY_SOURCE_H
