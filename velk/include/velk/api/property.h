#ifndef VELK_API_PROPERTY_H
#define VELK_API_PROPERTY_H

#include <velk/api/any.h>
#include <velk/api/velk.h>
#include <velk/common.h>
#include <velk/interface/intf_function.h>
#include <velk/interface/intf_property.h>
#include <velk/interface/intf_velk.h>
#include <velk/interface/types.h>

namespace velk {

namespace detail {

/** @brief Non-template storage for ConstProperty<T>. Compiled once regardless of T instantiations. */
class PropertyStorage
{
protected:
    PropertyStorage() = delete;
    explicit PropertyStorage(IProperty::Ptr existing) : prop_(std::move(existing)) {}

public:
    /** @brief Returns wrapped IProperty interface. */
    operator const IProperty::Ptr() { return prop_; }

    /** @brief Returns wrapped IProperty interface. */
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

protected:
    IPropertyInternal *get_internal() const { return interface_cast<IPropertyInternal>(prop_); }
    IProperty::Ptr prop_;
};

} // namespace detail

/**
 * @brief Read-only typed property wrapper with get_value accessor for type T.
 *
 * Returned by RPROP accessors. Provides read and observe operations but no set_value.
 * @tparam T The value type stored by the property.
 */
template<class T>
class ConstProperty : public detail::PropertyStorage
{
public:
    using Type = std::remove_const_t<T>;
    static constexpr Uid TYPE_UID = type_uid<Type>();

    /** @brief Wraps an existing IProperty pointer. */
    explicit ConstProperty(IProperty::Ptr existing) : PropertyStorage(std::move(existing)) {}

    /** @brief Returns the current value of the property. */
    T get_value() const {
        Type value{};
        if (auto internal = get_internal()) {
            if (auto any = internal->get_any()) {
                any->get_data(&value, sizeof(Type), TYPE_UID);
            }
        }
        return value;
    }
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
    using Type = typename Base::Type;

public:
    /** @brief Wraps an existing IProperty pointer. */
    explicit Property(IProperty::Ptr existing) : Base(std::move(existing)) {}

    /** @brief Sets the property to @p value. */
    ReturnValue set_value(const Type &value)
    {
        if (auto internal = this->get_internal()) {
            return internal->set_data(&value, sizeof(Type), Base::TYPE_UID);
        }
        return ReturnValue::FAIL;
    }
};

/**
 * @brief A helper template for creating a new read-only poperty instance
 */
template<class T, class = std::enable_if_t<std::is_const_v<T>>>
ConstProperty<std::remove_const_t<T>> create_property(const T &value = {})
{
    using Type = std::remove_const_t<T>;
    Any<Type> v(value);
    return ConstProperty<Type>(instance().create_property<Type>(v.clone(), ObjectFlags::ReadOnly));
}

/**
 * @brief A helper template for creating a new poperty instance
 */
template<class T, class = std::enable_if_t<!std::is_const_v<T>>>
Property<T> create_property(const T &value = {})
{
    Any<T> v(value);
    return Property<T>(instance().create_property<T>(v.clone()));
}

} // namespace velk

#endif // VELK_API_PROPERTY_H
