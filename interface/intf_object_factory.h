#ifndef INTF_OBJECT_FACTORY_H
#define INTF_OBJECT_FACTORY_H

#include <interface/intf_object.h>
#include <interface/types.h>

class IObjectFactory : public IInterface
{
    INTERFACE(IObjectFactory)
public:
    virtual IObject::Ptr CreateInstance() const = 0;
    virtual const ClassInfo &GetClassInfo() const = 0;
};

#endif // INTF_OBJECT_FACTORY_H
