#ifndef VELK_STORE_IMPL_H
#define VELK_STORE_IMPL_H

#include <velk/ext/object.h>
#include <velk/interface/intf_store.h>
#include <velk/string.h>
#include <velk/vector.h>

#include <shared_mutex>

namespace velk::impl {

/**
 * @brief Default implementation of IStore.
 *
 * Owns a flat collection of objects indexed by string id.
 * Linear lookup by id. Maintains insertion order for index-based access.
 * Thread-safe: reads use shared locks, mutations use exclusive locks.
 */
class Store final : public ext::Object<Store, IStore>
{
public:
    VELK_CLASS_UID(ClassId::Store, "Store");

    IObject::Ptr find(string_view id) const override;
    size_t object_count() const override;
    IObject::Ptr object_at(size_t index) const override;
    string id_at(size_t index) const override;
    ReturnValue add(string_view id, const IObject::Ptr& object) override;

    struct Entry
    {
        string id;
        IObject::Ptr object;
    };

private:

    mutable std::shared_mutex mutex_;
    vector<Entry> entries_;
};

} // namespace velk::impl

#endif // VELK_STORE_IMPL_H
