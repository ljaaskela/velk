#ifndef INTF_OBJECT_H
#define INTF_OBJECT_H

#include <common.h>
#include <interface/intf_interface.h>

namespace strata {

/** @brief Base interface for all Strata objects. */
class IObject : public Interface<IObject>
{
public:
};

/**
 * @brief Interface for objects that can retrieve a shared_ptr to themselves.
 *
 * Similar to std::enable_shared_from_this, but integrated with the Strata interface system.
 */
class ISharedFromObject : public Interface<ISharedFromObject>
{
public:
    /** @brief Stores the owning shared_ptr. Called by the factory after creation. */
    virtual void set_self(const IObject::Ptr &self) = 0;
    /** @brief Returns a shared_ptr to this object, or nullptr if expired. */
    virtual IObject::Ptr get_self() const = 0;
};

/**
 * @brief Casts a raw IObject pointer to a shared_ptr of the target interface type.
 * @tparam T The target interface type.
 * @param obj The raw object pointer.
 * @return A shared_ptr<T> if the cast succeeds, empty shared_ptr otherwise.
 */
template<class T>
typename T::Ptr interface_pointer_cast(IObject *obj)
{
    if (auto s = interface_pointer_cast<ISharedFromObject>(obj)) {
        return interface_pointer_cast<T>(s->get_self());
    }
    return {};
}

} // namespace strata

#endif // INTF_OBJECT_H
