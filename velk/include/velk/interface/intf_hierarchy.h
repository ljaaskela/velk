#ifndef VELK_INTF_HIERARCHY_H
#define VELK_INTF_HIERARCHY_H

#include <velk/interface/intf_object.h>
#include <velk/vector.h>

namespace velk {

class IHierarchy;

/** @brief Lightweight handle binding an object to its hierarchy. */
struct HierarchyNode
{
    IObject::Ptr object;                  ///< The object this node represents.
    weak_ptr<IHierarchy> hierarchy;       ///< Weak reference to the owning hierarchy.
};

/**
 * @brief Manages a single-root tree of IObject references.
 *
 * Hierarchy is external to objects: objects do not know about hierarchy,
 * hierarchy knows about objects. An object can participate in multiple
 * hierarchies simultaneously.
 *
 * Chain: IInterface -> IHierarchy
 */
class IHierarchy : public Interface<IHierarchy>
{
public:
    /** @brief Sets the root object of this hierarchy. Clears existing tree. */
    virtual ReturnValue set_root(const IObject::Ptr& root) = 0;

    /** @brief Appends a child to the given parent. */
    virtual ReturnValue add(const IObject::Ptr& parent, const IObject::Ptr& child) = 0;

    /** @brief Inserts a child at the given index under the parent. */
    virtual ReturnValue insert(const IObject::Ptr& parent, size_t index, const IObject::Ptr& child) = 0;

    /** @brief Removes an object and all its descendants from the hierarchy. */
    virtual ReturnValue remove(const IObject::Ptr& object) = 0;

    /** @brief Replaces an object in-place, preserving position and reparenting children. */
    virtual ReturnValue replace(const IObject::Ptr& old_child, const IObject::Ptr& new_child) = 0;

    /** @brief Removes all objects from the hierarchy. */
    virtual void clear() = 0;

    /** @brief Returns the root object, or null if none set. */
    virtual IObject::Ptr root() const = 0;

    /** @brief Returns a snapshot of the given object's position in the hierarchy. */
    virtual HierarchyNode node_of(const IObject::Ptr& object) const = 0;

    /** @brief Returns the parent of the given object, or null if root or not in hierarchy. */
    virtual IObject::Ptr parent_of(const IObject::Ptr& object) const = 0;

    /** @brief Returns the children of the given object. */
    virtual vector<IObject::Ptr> children_of(const IObject::Ptr& object) const = 0;

    /** @brief Returns the child at the given index, or null if out of range. */
    virtual IObject::Ptr child_at(const IObject::Ptr& object, size_t index) const = 0;

    /** @brief Returns the number of children of the given object. */
    virtual size_t child_count(const IObject::Ptr& object) const = 0;

    /** @brief Returns true if the object is in this hierarchy. */
    virtual bool contains(const IObject::Ptr& object) const = 0;

    /** @brief Returns the total number of objects in this hierarchy (including root). */
    virtual size_t size() const = 0;
};

} // namespace velk

#endif // VELK_INTF_HIERARCHY_H
