#ifndef VELK_INTF_CONTAINER_H
#define VELK_INTF_CONTAINER_H

#include <velk/interface/intf_object.h>
#include <velk/vector.h>

namespace velk {

/**
 * @brief Interface for an ordered collection of child objects.
 *
 * Enables object hierarchies (e.g. for editor UIs, scene graphs) by providing
 * a vector-like API for managing child objects.
 * Typically an IContainer is implemented by ClassId::Container and attached to an object
 * via IObjectStorage::add_attachment() to inject hierarchy awareness into an object.
 */
class IContainer : public Interface<IContainer>
{
public:
    /** @brief Appends a child to the end of the container. */
    virtual ReturnValue add(const IObject::Ptr& child) = 0;
    /** @brief Removes a child by pointer identity. */
    virtual ReturnValue remove(const IObject::Ptr& child) = 0;
    /** @brief Inserts a child at the given index. */
    virtual ReturnValue insert(size_t index, const IObject::Ptr& child) = 0;
    /** @brief Replaces the child at the given index. */
    virtual ReturnValue replace(size_t index, const IObject::Ptr& child) = 0;
    /** @brief Returns the child at the given index, or nullptr if out of range. */
    virtual IObject::Ptr get_at(size_t index) const = 0;
    /** @brief Returns all children as a vector. */
    virtual vector<IObject::Ptr> get_all() const = 0;
    /** @brief Returns the number of children. */
    virtual size_t size() const = 0;
    /** @brief Removes all children. */
    virtual void clear() = 0;
};

} // namespace velk

#endif // VELK_INTF_CONTAINER_H
