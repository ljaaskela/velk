#ifndef INTF_OBJECT_H
#define INTF_OBJECT_H

#include <common.h>
#include <interface/intf_function.h>
#include <interface/intf_interface.h>

class IObject : public Interface<IObject>
{
public:
};

class ISharedFromObject : public Interface<ISharedFromObject>
{
public:
    virtual void SetSelf(const IObject::Ptr &self) = 0;
    virtual IObject::Ptr GetSelf() const = 0;
};

template<class T>
typename T::Ptr interface_pointer_cast(IObject *obj)
{
    if (auto s = interface_pointer_cast<ISharedFromObject>(obj)) {
        return interface_pointer_cast<T>(s->GetSelf());
    }
    return {};
}

#endif // INTF_OBJECT_H
