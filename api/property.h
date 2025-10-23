#ifndef API_PROPERTY_H
#define API_PROPERTY_H

#include <api/any.h>
#include <api/function.h>
#include <common.h>
#include <interface/intf_metadata.h>
#include <interface/intf_property.h>
#include <interface/intf_registry.h>
#include <interface/types.h>
#include <iostream>

// Base property holder for an IProperty::Ptr
class Property
{
public:
    operator const IProperty::Ptr()
    {
        return prop_;
    }
    operator const IProperty::ConstPtr() const
    {
        return prop_;
    }
    operator bool() const
    {
        return prop_.operator bool();
    }
    const IProperty::Ptr GetPropertyInterface()
    {
        return prop_;
    }
    const IProperty::ConstPtr GetPropertyInterface() const
    {
        return prop_;
    }
    void AddOnChanged(const Function &fn)
    {
        if (prop_) {
            prop_->OnChanged()->AddHandler(fn);
        }
    }
    void RemoveOnChanged(const Function &fn)
    {
        if (prop_) {
            prop_->OnChanged()->RemoveHandler(fn);
        }
    }

    virtual Uid GetTypeUid() const = 0;

protected:
    void Create()
    {
        prop_ = GetRegistry().CreateProperty(GetTypeUid(), {});
        if (prop_) {
            internal_ = prop_->GetInterface<IPropertyInternal>();
        }
    }

    Property() = default;
    IProperty::Ptr prop_;
    IPropertyInternal *internal_{};
};

// Typed holder for an IProperty::Ptr
template<class T>
class PropertyT final : public Property
{
public:
    static constexpr Uid TYPE_UID = TypeUid<T>();
    PropertyT()
    {
        Create();
    }
    PropertyT(const T& value)
    {
        Create();
        Set(value);
    }
    Uid GetTypeUid() const override
    {
        return TYPE_UID;
    }
    T Get() const {
        if (internal_) {
            // Typed accessor reference to property's any
            return AnyT<T>(internal_->GetAny()).Get();
        }
        return {};
    }
    void Set(const T& value) {
        if (internal_) {
            // This is a bit suboptimal as we create a new any object to wrap the value
            auto v = AnyT<T>(value);
            prop_->SetValue(v);
        }
    }
};

template<class T>
class property_ptr_t : public property_ptr
{
public:
    T Get() { return prop_ ? AnyT<T>(prop_->GetValue()).Get() : T{}; }
    void Set(const T &value)
    {
        if (prop_) {
            // This is a bit suboptimal as we create a new any object to wrap the value
            prop_->SetValue(AnyT<T>(value));
        }
    }
};

#endif // PROPERTY_H
