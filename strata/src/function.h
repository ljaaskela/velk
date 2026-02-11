#ifndef FUNCTION_H
#define FUNCTION_H

#include "strata_impl.h"
#include <common.h>
#include <ext/core_object.h>
#include <interface/intf_function.h>
#include <interface/types.h>

namespace strata {

class FunctionImpl final : public CoreObject<FunctionImpl, IFunction, IFunctionInternal>
{
public:
    FunctionImpl() = default;
    ReturnValue invoke(const IAny *args) const override;
    void set_invoke_callback(IFunction::CallableFn *fn) override;
    void bind(void* context, IFunctionInternal::BoundFn* fn) override;

private:
    IFunction::CallableFn *fn_{};
    void* bound_context_{};
    IFunctionInternal::BoundFn* bound_fn_{};
};

} // namespace strata

#endif // FUNCTION_H
