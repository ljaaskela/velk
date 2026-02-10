#ifndef INTF_EXTERNAL_ANY_H
#define INTF_EXTERNAL_ANY_H

#include <common.h>
#include <interface/intf_event.h>

/**
 * @brief The IExternalAny class can be implemented by any objects whose data can change
 *        without a property setting its value.
 *        
 *        The default property implementation will check for IExternalAny and invoke
 *        IProperty::OnChanged when IExternalAny::OnDataChanged is invoked.
 */
class IExternalAny : public Interface<IExternalAny>
{
public:
    /**
     * @brief Invoked when the data changes externally
     */
    virtual IEvent::Ptr OnDataChanged() const = 0;
};

#endif // INTF_EXTERNAL_ANY_H
