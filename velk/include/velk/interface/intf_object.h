#ifndef VELK_INTF_OBJECT_H
#define VELK_INTF_OBJECT_H

#include <velk/common.h>
#include <velk/interface/intf_interface.h>

namespace velk {

/** @brief Base interface for all Velk objects. */
class IObject : public Interface<IObject>
{
public:
    /** @brief Returns the class UID of this object. */
    virtual Uid get_class_uid() const = 0;

    /** @brief Returns the name of the class. */
    virtual string_view get_class_name() const = 0;

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

#endif // VELK_INTF_OBJECT_H
