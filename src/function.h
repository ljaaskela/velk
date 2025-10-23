#ifndef FUNCTION_H
#define FUNCTION_H

#include "registry.h"
#include <common.h>
#include <ext/object.h>
#include <interface/intf_function.h>
#include <interface/types.h>

class FunctionImpl final : public Object<FunctionImpl, IFunction, IFunctionInternal>
{
    IMPLEMENT_CLASS(ClassId::Function)
public:
    FunctionImpl() = default;
    ReturnValue Invoke(const IAny *args) const override;
    void SetInvokeCallback(IFunction::CallableFn *fn) override;

private:
    IFunction::CallableFn *fn_{};
};

#endif // FUNCTION_H
