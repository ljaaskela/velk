#ifndef INTF_METADATA_H
#define INTF_METADATA_H

#include <interface/intf_interface.h>
#include <interface/intf_property.h>

#include <memory>
#include <string_view>

class IMetaData;

class property_ptr : public std::shared_ptr<IMetaData>
{
    explicit property_ptr(IProperty *p) : prop_(p) {}
    IProperty *GetPropertyPtr() { return prop_; }

protected:
    IProperty *prop_{};
};

class IMetaData : public IInterface
{
    INTERFACE(IMetaData)
public:
    virtual property_ptr GetProperty(const std::string_view name) = 0;
};

#endif // INTF_METADATA_H
