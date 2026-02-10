#include "function.h"

ReturnValue FunctionImpl::Invoke(const IAny *args) const
{
    if (fn_) {
        return fn_(args);
    }
    return ReturnValue::NOTHING_TO_DO;
}

void FunctionImpl::SetInvokeCallback(IFunction::CallableFn *fn)
{
    fn_ = fn;
}
