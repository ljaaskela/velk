#ifndef INTF_FUNCTION_H
#define INTF_FUNCTION_H

#include <common.h>
#include <interface/intf_any.h>
#include <interface/intf_interface.h>

class IFunction : public InterfaceBase<IFunction>
{
public:
    using CallableFn = ReturnValue(const IAny *);
    /**
     * @brief Called to invoke the function
     * @param args Call args. Actual content dependent on the function implementation.
     * @return Function return value.
     */
    virtual ReturnValue Invoke(const IAny *args) const = 0;
};

class IFunctionInternal : public InterfaceBase<IFunctionInternal>
{
public:
    virtual void SetInvokeCallback(IFunction::CallableFn *fn) = 0;
};

#endif // INTF_FUNCTION_H
