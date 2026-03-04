#ifndef VELK_INTF_ANY_EXTENSION_H
#define VELK_INTF_ANY_EXTENSION_H

#include <velk/interface/intf_any.h>

namespace velk {

/**
 * @brief Interface for chainable IAny wrappers that intercept property value get/set.
 *
 * Inherits IAny so it can sit directly in PropertyImpl's data_ slot. Each extension
 * wraps an inner IAny (which may itself be another extension), forming a chain.
 * When no extensions are present there is zero overhead.
 */
class IAnyExtension : public Interface<IAnyExtension, IAny>
{
public:
    /** @brief Returns the inner IAny this extension wraps (const). */
    virtual IAny::ConstPtr get_inner() const = 0;
    /** @brief Takes ownership of the inner IAny, leaving this extension empty. */
    virtual IAny::Ptr take_inner(IInterface& owner) = 0;
    /** @brief Sets the inner IAny this extension wraps. */
    virtual void set_inner(IAny::Ptr inner, const IInterface::WeakPtr& owner) = 0;
};

} // namespace velk

#endif // VELK_INTF_ANY_EXTENSION_H
