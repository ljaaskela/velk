#ifndef INTF_METADATA_H
#define INTF_METADATA_H

#include <interface/intf_interface.h>
#include <interface/intf_property.h>

#include <memory>
#include <string_view>

class IMetaData;

/** @brief A shared_ptr to IMetaData that also holds a reference to the underlying property. */
class property_ptr : public std::shared_ptr<IMetaData>
{
    explicit property_ptr(IProperty *p) : prop_(p) {}
    IProperty *GetPropertyPtr() { return prop_; }

protected:
    IProperty *prop_{};
};

/** @brief Interface for querying object properties by name. */
class IMetaData : public Interface<IMetaData>
{
public:
    /** @brief Returns a property_ptr for the named property, or empty if not found. */
    virtual property_ptr GetProperty(const std::string_view name) = 0;
};

#endif // INTF_METADATA_H
