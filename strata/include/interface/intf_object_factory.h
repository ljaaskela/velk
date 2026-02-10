#ifndef INTF_OBJECT_FACTORY_H
#define INTF_OBJECT_FACTORY_H

#include <interface/intf_object.h>
#include <interface/types.h>

namespace strata {

/** @brief Interface for factories that create IObject instances of a specific class. */
class IObjectFactory : public Interface<IObjectFactory>
{
public:
    /** @brief Creates a new instance of the class this factory represents. */
    virtual IObject::Ptr CreateInstance() const = 0;
    /** @brief Returns the ClassInfo describing the class this factory creates. */
    virtual const ClassInfo &GetClassInfo() const = 0;
};

} // namespace strata

#endif // INTF_OBJECT_FACTORY_H
