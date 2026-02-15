#ifndef API_PROPERTY_H
#define API_PROPERTY_H

#include <api/strata.h>
#include <common.h>
#include <interface/intf_function.h>
#include <interface/intf_property.h>
#include <interface/intf_strata.h>
#include <interface/types.h>

namespace strata {

/**
 * @brief Read-only typed property wrapper with get_value accessor for type T.
 *
 * Returned by RPROP accessors. Provides read and observe operations but no set_value.
 * @tparam T The value type stored by the property.
 */
template<class T>
class ConstProperty
{
public:
    static constexpr Uid TYPE_UID = type_uid<T>();

    /** @brief Wraps an existing IProperty pointer. */
    explicit ConstProperty(IProperty::Ptr existing) : prop_(std::move(existing)) {}

    operator const IProperty::Ptr() { return prop_; }
    operator const IProperty::ConstPtr() const { return prop_; }

    /** @brief Returns true if the underlying IProperty is valid. */
    operator bool() const { return prop_.operator bool(); }

    /** @brief Returns the underlying IProperty pointer. */
    const IProperty::Ptr get_property_interface() { return prop_; }

    /** @copydoc get_property_interface() */
    const IProperty::ConstPtr get_property_interface() const { return prop_; }

    /** @brief Subscribes @p fn to be called when the property value changes. */
    void add_on_changed(const IFunction::ConstPtr &fn)
    {
        if (prop_) {
            prop_->on_changed()->add_handler(fn);
        }
    }

    /** @brief Unsubscribes @p fn from property change notifications. */
    void remove_on_changed(const IFunction::ConstPtr &fn)
    {
        if (prop_) {
            prop_->on_changed()->remove_handler(fn);
        }
    }

    /** @brief Returns the current value of the property. */
    T get_value() const {
        T value{};
        if (auto internal = get_internal()) {
            if (auto any = internal->get_any()) {
                any->get_data(&value, sizeof(T), TYPE_UID);
            }
        }
        return value;
    }

protected:
    ConstProperty() = default;
    IPropertyInternal *get_internal() const { return interface_cast<IPropertyInternal>(prop_); }
    IProperty::Ptr prop_;
};

/**
 * @brief Typed property wrapper with get_value/set_value accessors for type T.
 *
 * Returned by PROP accessors. Inherits read/observe from ConstProperty<T> and adds set_value.
 * @tparam T The value type stored by the property.
 */
template<class T>
class Property : public ConstProperty<T>
{
    using Base = ConstProperty<T>;
public:
    /** @brief Default-constructs a property of type T via Strata. */
    Property() { this->prop_ = instance().create_property(Base::TYPE_UID, {}); }

    /** @brief Constructs a property of type T and sets its initial value. */
    Property(const T& value)
    {
        this->prop_ = instance().create_property(Base::TYPE_UID, {});
        set_value(value);
    }

    /** @brief Wraps an existing IProperty pointer. */
    explicit Property(IProperty::Ptr existing) : Base(std::move(existing)) {}

    /** @brief Sets the property to @p value. */
    void set_value(const T& value) {
        if (auto internal = this->get_internal()) {
            internal->set_data(&value, sizeof(T), Base::TYPE_UID);
        }
    }
};

} // namespace strata

#endif // API_PROPERTY_H
