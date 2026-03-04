#ifndef VELK_INTF_OBJECT_STORAGE_H
#define VELK_INTF_OBJECT_STORAGE_H

#include <velk/interface/intf_metadata.h>

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
};

} // namespace velk

#endif // VELK_INTF_OBJECT_STORAGE_H
