#include "function.h"

namespace strata {

ReturnValue FunctionImpl::invoke(const IAny *args) const
{
    if (fn_) {
        return fn_(args);
    }
    if (bound_fn_) {
        return bound_fn_(bound_context_, args);
    }
    return ReturnValue::NOTHING_TO_DO;
}

void FunctionImpl::set_invoke_callback(IFunction::CallableFn *fn)
{
    fn_ = fn;
}

void FunctionImpl::bind(void* context, IFunctionInternal::BoundFn* fn)
{
    bound_context_ = context;
    bound_fn_ = fn;
}

} // namespace strata
