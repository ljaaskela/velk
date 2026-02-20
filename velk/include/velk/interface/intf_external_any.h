#ifndef VELK_INTF_EXTERNAL_ANY_H
#define VELK_INTF_EXTERNAL_ANY_H

#include <velk/common.h>
#include <velk/interface/intf_event.h>

namespace velk {

/**
 * @brief The IExternalAny class can be implemented by any objects whose data can change
 *        without a property setting its value.
 *
 *        The default property implementation will check for IExternalAny and invoke
 *        IProperty::on_changed when IExternalAny::on_data_changed is invoked.
 */
class IExternalAny : public Interface<IExternalAny>
{
public:
    /**
     * @brief Invoked when the data changes externally
     */
    virtual IEvent::Ptr on_data_changed() const = 0;
};

} // namespace velk

#endif // VELK_INTF_EXTERNAL_ANY_H
