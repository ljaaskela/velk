#ifndef VELK_INTF_OBJECT_FACTORY_H
#define VELK_INTF_OBJECT_FACTORY_H

#include <velk/interface/intf_object.h>
#include <velk/interface/types.h>

namespace velk {

/** @brief Interface for factories that create IObject instances of a specific class. */
class IObjectFactory : public Interface<IObjectFactory>
{
public:
    /** @brief Creates a new instance of the class this factory represents. */
    virtual IObject::Ptr create_instance() const = 0;
    /** @brief Returns the ClassInfo describing the class this factory creates. */
    virtual const ClassInfo &get_class_info() const = 0;
    /** @brief Type helper create_instance(). */
    template<class T>
    typename T::Ptr create_instance() const
    {
        return interface_pointer_cast<T>(create_instance());
    }
};

} // namespace velk

#endif // VELK_INTF_OBJECT_FACTORY_H
