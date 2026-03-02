#ifndef VELK_INTF_PROPERTY_H
#define VELK_INTF_PROPERTY_H

#include <velk/interface/intf_any_extension.h>
#include <velk/interface/intf_event.h>
#include <velk/interface/intf_function.h>
#include <velk/vector.h>

namespace velk {

/** @brief Interface for a type-erased property with change notification. */
class IProperty : public Interface<IProperty>
{
public:
    /**
     * @brief Sets the property to a given value
     * @param from The value to set from.
     * @return ReturnValue::Success if value changed
     *         ReturnValue::NothingToDo if the same value was set
     *         ReturnValue::Fail otherwise
     */
    virtual ReturnValue set_value(const IAny& from, InvokeType type = Immediate) = 0;
    /**
     * @brief Returns the property's current value.
     */
    virtual const IAny::ConstPtr get_value() const = 0;
    /**
     * @brief Invoked when value of the property changes as a response to
     *        set_value being called.
     */
    virtual IEvent::Ptr on_changed() const = 0;
};

/** @brief Internal interface for initializing a property's backing IAny storage. */
class IPropertyInternal : public Interface<IPropertyInternal, IProperty>
{
public:
    /**
     * @brief Sets the internal any object for a property instance.
     * @param any The any object to set.
     * @param previous If non-null and data_ was already set, receives the old any object.
     * @return true if successful.
     */
    virtual bool set_any(const IAny::Ptr& any, IAny::Ptr* previous = nullptr) = 0;
    /**
     * @brief Returns the internal any object.
     * @note  Any changes through the direct any accessor will not lead to
     *        IProperty::on_changed being invoked.
     */
    virtual IAny::ConstPtr get_any() const = 0;
    /**
     * @brief Writes raw data directly into the backing IAny and fires on_changed if the value changed.
     * @param data Pointer to the source data.
     * @param size Size of the source data in bytes.
     * @param type Type UID of the data.
     */
    virtual ReturnValue set_data(const void* data, size_t size, Uid type,
                                 InvokeType invokeType = Immediate) = 0;
    /**
     * @brief Copies @p from into the backing value without firing on_changed.
     * @return ReturnValue::Success if the value changed and the caller should fire on_changed.
     *         ReturnValue::NothingToDo if unchanged or if the property is externally notified.
     */
    virtual ReturnValue set_value_silent(const IAny& from) = 0;
    /**
     * @brief Installs an IAnyExtension at the head of the property's any chain.
     * @param extension Must implement IAnyExtension. Its inner will be set to the current data.
     * @return true if the extension was installed successfully.
     */
    virtual bool install_extension(const IAnyExtension::Ptr& extension) = 0;
    /**
     * @brief Removes a previously installed IAnyExtension from the property's any chain.
     * @param extension The extension to remove. Must be in the chain.
     * @return true if the extension was found and removed.
     */
    virtual bool remove_extension(const IAnyExtension::Ptr& extension) = 0;
};

/**
 * @brief Walks the IAnyExtension chain of a property's value, collecting each node.
 * @param property The property whose value chain to walk.
 * @return A vector of all IAny pointers in the chain (extensions + final plain IAny).
 */
inline vector<IAny::ConstPtr> get_any_chain(const IProperty& property)
{
    vector<IAny::ConstPtr> chain;
    auto current = property.get_value();
    while (current) {
        chain.push_back(current);
        if (auto ext = interface_cast<IAnyExtension>(current)) {
            current = ext->get_inner();
        } else {
            break;
        }
    }
    return chain;
}

} // namespace velk

#endif
