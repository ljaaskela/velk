#ifndef VELK_INTF_STORE_H
#define VELK_INTF_STORE_H

#include <velk/interface/intf_object.h>
#include <velk/interface/types.h>
#include <velk/string.h>

namespace velk {

/**
 * @brief Owns a flat collection of imported objects. Provides lookup by id and enumeration.
 *
 * The implementation (StoreImpl) inherits ext::Object which gives it IObjectStorage
 * and attachment support, but IStore itself stays minimal.
 *
 * Chain: IInterface -> IStore
 */
class IStore : public Interface<IStore>
{
public:
    /** @brief Returns an object by its string id, or nullptr if not found. */
    virtual IObject::Ptr find(string_view id) const = 0;
    /** @brief Returns the number of objects in the store. */
    virtual size_t object_count() const = 0;
    /** @brief Returns the object at the given index, or nullptr if out of range. */
    virtual IObject::Ptr object_at(size_t index) const = 0;
    /** @brief Returns the id of the object at the given index, or an empty string if out of range. */
    virtual string id_at(size_t index) const = 0;
    /** @brief Adds an object with the given id. Returns Fail if id already exists. */
    virtual ReturnValue add(string_view id, const IObject::Ptr& object) = 0;
};

} // namespace velk

#endif // VELK_INTF_STORE_H
