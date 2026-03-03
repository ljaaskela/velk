#ifndef VELK_API_ATTACHMENT_H
#define VELK_API_ATTACHMENT_H

#include <velk/api/velk.h>
#include <velk/interface/intf_object_storage.h>

namespace velk {

/**
 * @brief Finds an existing attachment of type T, or creates one by class UID and attaches it.
 * @tparam T The interface type to search for and return.
 * @param storage The object storage to search and attach to.
 * @param classUid The class UID used to create a new instance if none is found.
 * @return Shared pointer to the attachment cast to T, or nullptr on failure.
 */
template <class T>
typename T::Ptr find_or_create_attachment(IObjectStorage* storage, Uid classUid)
{
    return storage ? storage->template find_attachment<T>(classUid) : typename T::Ptr{};
}

/**
 * @brief Finds an existing attachment of type T, or creates one by class UID and attaches it.
 * @tparam T The interface type to search for and return.
 * @param obj The object to search for an IObjectStorage interface on.
 * @param classUid The class UID used to create a new instance if none is found.
 * @return Shared pointer to the attachment cast to T, or nullptr if obj has no IObjectStorage.
 */
template <class T>
typename T::Ptr find_or_create_attachment(IInterface* obj, Uid classUid)
{
    return find_or_create_attachment<T>(interface_cast<IObjectStorage>(obj), classUid);
}

} // namespace velk

#endif // VELK_API_ATTACHMENT_H
