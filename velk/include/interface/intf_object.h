#ifndef INTF_OBJECT_H
#define INTF_OBJECT_H

#include <common.h>
#include <interface/intf_interface.h>

namespace velk {

/** @brief Base interface for all Velk objects. */
class IObject : public Interface<IObject>
{
public:
    /** @brief Returns a shared_ptr to this object, or empty if not available. */
    virtual Ptr get_self() const = 0;
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
    if (!obj) return {};
    return interface_pointer_cast<T>(obj->get_self());
}

} // namespace velk

#endif // INTF_OBJECT_H
