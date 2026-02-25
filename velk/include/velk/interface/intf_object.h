#ifndef VELK_INTF_OBJECT_H
#define VELK_INTF_OBJECT_H

#include <velk/common.h>
#include <velk/interface/intf_interface.h>
#include <velk/interface/types.h>

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
 * @brief Returns a shared_ptr to the object, optionally cast to interface T.
 * @tparam T The target interface type (defaults to IObject).
 * @param object The object to retrieve the self pointer from.
 */
template <class T = IObject, class U>
typename T::Ptr get_self(U* object)
{
    auto* obj = interface_cast<IObject>(object);
    return obj ? interface_pointer_cast<T>(obj->get_self()) : typename T::Ptr{};
}

} // namespace velk

#endif // VELK_INTF_OBJECT_H
