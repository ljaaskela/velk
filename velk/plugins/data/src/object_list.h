#ifndef VELK_DATA_OBJECT_LIST_H
#define VELK_DATA_OBJECT_LIST_H

#include <velk/ext/object.h>
#include <velk/plugins/data/interface/intf_object_list.h>
#include <velk/plugins/data/plugin.h>

#include <mutex>

namespace velk::impl {

/** @brief See IObjectList. Thread-safe; events fire outside the lock. */
class ObjectList final : public ext::Object<ObjectList, IObjectList>
{
public:
    VELK_CLASS_UID(ClassId::ObjectList, "ObjectList");

    // IItemSource
    size_t child_count(const IObject::Ptr& parent) const override;
    IObject::Ptr child_at(const IObject::Ptr& parent, size_t index) const override;
    bool has_children(const IObject::Ptr&) const override { return false; }

    // IObjectList
    size_t size() const override;
    IObject::Ptr at(size_t index) const override;
    size_t index_of(const IObject::Ptr& item) const override;
    ReturnValue add(const IObject::Ptr& item) override;
    ReturnValue insert(size_t index, array_view<const IObject::Ptr> items) override;
    ReturnValue remove(size_t index, size_t count) override;
    ReturnValue move(size_t from, size_t to) override;
    void clear() override;

private:
    void fire(ItemChange change);

    mutable std::mutex mutex_;
    vector<IObject::Ptr> items_;
};

} // namespace velk::impl

#endif // VELK_DATA_OBJECT_LIST_H
