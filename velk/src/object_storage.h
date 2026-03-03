#ifndef OBJECT_STORAGE_H
#define OBJECT_STORAGE_H

#include <velk/ext/refcounted_dispatch.h>
#include <velk/interface/intf_object_storage.h>

#include <memory>
#include <vector>

namespace velk {

/**
 * @brief Runtime object storage that lazily creates property/event/function instances
 *        and supports arbitrary attachments.
 *
 * Holds a static array_view<MemberDesc> (from VELK_INTERFACE) and creates runtime
 * PropertyImpl/FunctionImpl instances on first access via get_property()/get_event()/
 * get_function(). Created instances are cached for subsequent lookups.
 *
 * The instances_ vector is partitioned: [0, attachment_end_) holds attachments,
 * [attachment_end_, size()) holds metadata instances. Attachment entries use SIZE_MAX
 * as the index sentinel.
 *
 * Does not inherit RefCountedDispatch; lifetime is managed by the owning Object
 * (allocated by VelkInstance at construction, deleted in Object's destructor).
 */
class ObjectStorage final : public ext::InterfaceDispatch<IObjectStorage>
{
public:
    /**
     * @brief Constructs an object storage from static member descriptors.
     * @param members The static metadata array (from VELK_INTERFACE).
     * @param owner The owning object, used to bind function trampolines and resolve property state.
     */
    explicit ObjectStorage(array_view<MemberDesc> members, IInterface* owner = nullptr);

public: // IObject (inherited via IObjectStorage; not used as an IObject)
    Uid get_class_uid() const override { return {}; }
    string_view get_class_name() const override { return {}; }
    IObject::Ptr get_self() const override { return {}; }
    uint32_t get_object_flags() const override { return 0; }

public: // IPropertyState (inherited via IMetadata; state lives in the Object, not here)
    void* get_property_state(Uid) override { return nullptr; }

public: // IMetadata
    array_view<MemberDesc> get_static_metadata() const override;
    IProperty::Ptr get_property(string_view name, Resolve mode = Resolve::Create) const override;
    IEvent::Ptr get_event(string_view name, Resolve mode = Resolve::Create) const override;
    IFunction::Ptr get_function(string_view name, Resolve mode = Resolve::Create) const override;
    void notify(MemberKind kind, Uid interfaceUid, Notification notification) const override;

public: // IObjectStorage (attachment operations)
    ReturnValue add_attachment(const IInterface::Ptr& attachment) override;
    ReturnValue remove_attachment(const IInterface::Ptr& attachment) override;
    size_t attachment_count() const override;
    IInterface::Ptr get_attachment(size_t index) const override;
    IInterface::Ptr find_attachment(const AttachmentQuery& query, Resolve mode) override;

private:
    array_view<MemberDesc> members_; ///< Static metadata descriptors from VELK_INTERFACE.
    IInterface* owner_{};            ///< Owning object for trampoline binding and state access.

    /// Lazily populated cache: [0, attachment_end_) = attachments (idx == SIZE_MAX),
    ///                         [attachment_end_, size()) = metadata instances.
    mutable std::vector<std::pair<size_t, IInterface::Ptr>> instances_;
    mutable uint32_t attachment_end_{0}; ///< Boundary between attachments and metadata entries.

    /** @brief Finds a static member by name and kind, creating its runtime instance if needed. */
    IInterface::Ptr find_or_create(string_view name, MemberKind kind, Resolve mode) const;
    /** @brief Creates a runtime instance (PropertyImpl or FunctionImpl) from a member descriptor. */
    IInterface::Ptr create(MemberDesc desc) const;
    /** @brief Binds a function instance to the owner's virtual trampoline. */
    void bind(const MemberDesc& m, const IInterface::Ptr& fn) const;
};

} // namespace velk

#endif // OBJECT_STORAGE_H
