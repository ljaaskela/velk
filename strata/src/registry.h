#ifndef REGISTRY_H
#define REGISTRY_H

#include <common.h>
#include <ext/object.h>
#include <interface/intf_registry.h>
#include <map>

class Registry final : public Object<Registry, IRegistry>
{
public:
    Registry();

    ReturnValue RegisterType(const IObjectFactory &factory) override;
    ReturnValue UnregisterType(const IObjectFactory &factory) override;
    IInterface::Ptr Create(Uid uid) const override;

    const ClassInfo* GetClassInfo(Uid classUid) const override;
    IAny::Ptr CreateAny(Uid type) const override;
    IProperty::Ptr CreateProperty(Uid type, const IAny::Ptr& value) const override;

private:
    std::map<Uid, const IObjectFactory *> types_;
};

#endif // REGISTRY_H
