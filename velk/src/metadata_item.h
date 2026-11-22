#ifndef METADATA_ITEM_H
#define METADATA_ITEM_H

#include "object_storage.h"

#include <velk/ext/core_object.h>

namespace velk::impl {

/**
 * @brief CRTP base for metadata member implementations (Property, Function, Event).
 *
 * Provides the IStorageOwned implementation and MemberSlot access. Storage-owned
 * items access their slot via ObjectStorage. Standalone items (no owner) lazily
 * allocate a heap MemberSlot on first access, identified by member_index_ == Standalone.
 *
 * @tparam FinalClass The concrete impl class (CRTP parameter).
 * @tparam Interfaces The interfaces the class implements.
 */
template <class FinalClass, class... Interfaces>
class MetadataItem : public ext::ObjectCore<FinalClass, Interfaces...>
{
protected:
    static constexpr uint16_t Standalone = uint16_t(-1);

public: // IStorageOwned
    IObjectStorage* get_owner() const override { return is_standalone() ? nullptr : owner_; }
    uint16_t get_member_index() const override { return member_index_; }
    uint16_t get_storage_id() const override { return storage_id_; }
    bool binding_stale() const override { return binding_stale_; }
    void set_binding_stale(bool stale) override { binding_stale_ = stale; }
    void set_owner(IObjectStorage* storage, uint16_t member_index, uint16_t storage_id) override
    {
        if (is_standalone() || owner_ == storage) {
            owner_ = storage;
            member_index_ = member_index;
            storage_id_ = storage_id;
        }
    }

protected:
    bool is_standalone() const { return member_index_ == Standalone; }

    MemberSlot& slot() const
    {
        if (!is_standalone()) {
            return static_cast<ObjectStorage*>(owner_)->slot(storage_id_);
        }
        if (!standalone_slot_) {
            standalone_slot_ = new MemberSlot();
        }
        return *standalone_slot_;
    }
    MemberData& data() const { return slot().data; }

    /** @brief Returns true if a standalone slot has been allocated. */
    bool has_standalone_slot() const { return is_standalone() && standalone_slot_ != nullptr; }

    /** @brief Deletes the standalone slot if this item owns one. */
    void delete_standalone_slot()
    {
        if (is_standalone()) {
            delete standalone_slot_;
            standalone_slot_ = nullptr;
        }
    }

private:
    union
    {
        IObjectStorage* owner_{};
        mutable MemberSlot* standalone_slot_;
    };
    uint16_t member_index_{Standalone};
    uint16_t storage_id_{};
    bool binding_stale_{}; ///< In the padding after the indices; see the assert below.
};

namespace metadata_item_layout {
struct MetadataItemFieldsBefore
{
    void* owner;
    uint16_t member_index;
    uint16_t storage_id;
};
struct MetadataItemFields
{
    void* owner;
    uint16_t member_index;
    uint16_t storage_id;
    bool binding_stale;
};
// The stale mark takes no extra memory.
static_assert(sizeof(MetadataItemFields) == sizeof(MetadataItemFieldsBefore));
} // namespace metadata_item_layout

} // namespace velk::impl

#endif // METADATA_ITEM_H
