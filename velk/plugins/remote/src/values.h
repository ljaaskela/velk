#ifndef VELK_REMOTE_VALUES_H
#define VELK_REMOTE_VALUES_H

#include <velk/interface/intf_any.h>
#include <velk/interface/intf_metadata.h>
#include <velk/interface/intf_object.h>
#include <velk/plugins/remote/types.h>

#include <unordered_map>

namespace velk::remote {

/**
 * @brief Gives objects handles that are never reused.
 *
 * Holds objects weakly. An object freed and another allocated at the same
 * address get different handles.
 */
class HandleTable
{
public:
    /** @brief The object's handle, assigned on first sight. 0 for null. */
    RemoteHandle handle_of(const IObject::Ptr& object);

    /** @brief The object's handle if it has one and is alive, else 0. Never assigns. */
    RemoteHandle find(const IObject* object) const;

    /** @brief The live object of @p handle, or null. */
    IObject::Ptr object_of(RemoteHandle handle) const;

    /** @brief The handle and class of @p object, assigning the handle on first sight. */
    RemoteObject describe(const IObject::Ptr& object);

    /** @brief Drops the entries of dead objects once the table has doubled since the last time. */
    void purge();

private:
    struct Entry
    {
        RemoteHandle handle;
        IObject::WeakPtr object;
    };
    std::unordered_map<const IObject*, Entry> by_object_;
    std::unordered_map<RemoteHandle, IObject::WeakPtr> by_handle_;
    RemoteHandle next_ = 1;
    size_t purge_at_ = 1024;
};

/**
 * @brief Encodes @p any, a value of @p type.
 * @param element_type The element type when @p any is an array, otherwise empty.
 */
ReturnValue encode_value(const IAny& any, Uid type, Uid element_type, HandleTable& handles, RemoteValue& out);

/** @brief Makes a value from @p value, or null when it cannot. */
IAny::Ptr decode_value(const RemoteValue& value, HandleTable& handles);

/** @brief Encodes the current value of a property. @p interface_uid as for IMetadata::get_property. */
ReturnValue encode_property(IMetadata& meta, string_view name, Uid interface_uid, HandleTable& handles,
                            RemoteValue& out);

} // namespace velk::remote

#endif // VELK_REMOTE_VALUES_H
