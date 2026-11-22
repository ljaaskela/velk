#ifndef VELK_INTF_OBJECT_STORAGE_H
#define VELK_INTF_OBJECT_STORAGE_H

#include <velk/interface/intf_metadata.h>
#include <velk/interface/intf_metadata_observer.h>

namespace velk {

/** @brief Query parameters for finding attachments. Zero means "don't filter by this field". */
struct AttachmentQuery
{
    Uid interfaceUid{}; ///< If set, attachment must implement this interface.
    Uid classUid{};     ///< If set, attachment must have this class UID. Used to create on miss.
};

/**
 * @brief Extension of IMetadata that supports arbitrary attachments alongside metadata members.
 *
 * IObjectStorage adds an attachment mechanism to the standard metadata container. Attachments
 * are IInterface::Ptr instances that can be added/removed/queried by index or by interface type.
 * This enables decorators, and other extension
 * patterns without modifying the core object system.
 *
 * Chain: IInterface -> IObject -> IPropertyState -> IMetadata -> IObjectStorage
 */
class IObjectStorage : public Interface<IObjectStorage, IMetadata>
{
public:
    /** @brief Adds an attachment to this object's storage. */
    virtual ReturnValue add_attachment(const IInterface::Ptr& attachment) = 0;
    /** @brief Removes an attachment by pointer identity. */
    virtual ReturnValue remove_attachment(const IInterface::Ptr& attachment) = 0;
    /** @brief Returns the number of attachments. */
    virtual size_t attachment_count() const = 0;
    /** @brief Returns the attachment at the given index, or nullptr if out of range. */
    virtual IInterface::Ptr get_attachment(size_t index) const = 0;

    /**
     * @brief Finds an attachment matching the query.
     *
     * With Resolve::Existing, searches only. With Resolve::Create, creates a new
     * instance via classUid if no match is found and attaches it.
     */
    virtual IInterface::Ptr find_attachment(const AttachmentQuery& query,
                                            Resolve mode = Resolve::Existing) = 0;

    /**
     * @brief Finds all attachments matching the query.
     */
    virtual vector<IInterface::Ptr> find_attachments(const AttachmentQuery& query) const = 0;

    /**
     * @brief Finds the first attachment that implements interface T.
     * @tparam T The interface type to search for.
     * @return A shared pointer to the attachment cast to T, or nullptr if not found.
     */
    template <class T>
    typename T::Ptr find_attachment() const
    {
        return interface_pointer_cast<T>(const_cast<IObjectStorage*>(this)->find_attachment({T::UID}));
    }

    /**
     * @brief Finds the first attachment that implements T, or creates one by classUid.
     * @tparam T The interface type to search for and return.
     * @param classUid The class UID used to create a new instance if none is found.
     * @return A shared pointer to the attachment cast to T, or nullptr on failure.
     */
    template <class T>
    typename T::Ptr find_attachment(Uid classUid)
    {
        return interface_pointer_cast<T>(find_attachment({T::UID, classUid}, Resolve::Create));
    }

    /**
     * @brief Returns the on_changed event for a property by storage id.
     * @param storage_id The member index within the static metadata array.
     * @param mode Resolve::Create allocates the event if absent; Resolve::Existing returns nullptr.
     */
    virtual IEvent::Ptr get_property_event(size_t storage_id, Resolve mode) const = 0;
    /**
     * @brief Fires the on_changed event for a property and notifies object-level observers.
     * @param storage_id The member index (uses Resolve::Existing so no event is created if nobody
     * subscribed).
     * @param property Direct pointer to the property with matching storage id.
     *        If nullptr, a property with @p storage_id is search from storage.
     */
    virtual void invoke_property_changed(size_t storage_id, IProperty* property) const = 0;
    /**
     * @brief Marks the bound property with @p storage_id as out of date in the object's state.
     *        Internal: called by bindings when their source changes.
     */
    virtual void mark_binding_stale(size_t storage_id) = 0;
    /**
     * @brief Copies the bound values of @p interfaceUid's out-of-date properties into the state.
     *        Internal: called when the state is read.
     * @return True when properties of other interfaces are still out of date.
     */
    virtual bool refresh_stale_bindings(Uid interfaceUid) = 0;
    /** @brief Registers an observer for property change notifications on this object. */
    virtual void add_observer(IMetadataObserver* observer) = 0;
    /** @brief Removes a previously registered observer. */
    virtual void remove_observer(IMetadataObserver* observer) = 0;
};

/** @brief Null-safe helper to get a property event by storage id. */
inline IEvent::Ptr get_property_event(IObjectStorage* storage, size_t storage_id, Resolve mode)
{
    return storage ? storage->get_property_event(storage_id, mode) : IEvent::Ptr{};
}

/** @brief Null-safe helper to fire property changed on the owner. */
inline void invoke_property_changed(IObjectStorage* storage, size_t storage_id, IProperty* property = nullptr)
{
    if (storage) {
        storage->invoke_property_changed(storage_id, property);
    }
}

/** @brief Finds the first attachment implementing T on @p storage. Null-safe. */
template <class T>
typename T::Ptr find_attachment(IObjectStorage* storage)
{
    return storage ? storage->template find_attachment<T>() : typename T::Ptr{};
}

/** @brief Finds the first attachment implementing T on @p obj's IObjectStorage. */
template <class T>
typename T::Ptr find_attachment(IInterface* obj)
{
    return find_attachment<T>(interface_cast<IObjectStorage>(obj));
}

/** @brief Finds the first attachment implementing T on the object behind @p obj. */
template <class T>
typename T::Ptr find_attachment(const IInterface::Ptr& obj)
{
    return find_attachment<T>(obj.get());
}

/** @brief True iff @p storage carries at least one T-implementing attachment. */
template <class T>
bool has_attachment(IObjectStorage* storage)
{
    return storage && storage->template find_attachment<T>() != nullptr;
}

/** @brief True iff @p obj's IObjectStorage carries at least one T-implementing attachment. */
template <class T>
bool has_attachment(IInterface* obj)
{
    return has_attachment<T>(interface_cast<IObjectStorage>(obj));
}

/** @brief True iff the object behind @p obj carries at least one T-implementing attachment. */
template <class T>
bool has_attachment(const IInterface::Ptr& obj)
{
    return has_attachment<T>(obj.get());
}

} // namespace velk

#endif // VELK_INTF_OBJECT_STORAGE_H
