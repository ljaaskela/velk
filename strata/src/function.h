#ifndef FUNCTION_H
#define FUNCTION_H

#include "strata_impl.h"
#include <common.h>
#include <ext/core_object.h>
#include <interface/intf_event.h>
#include <interface/intf_function.h>
#include <interface/types.h>

namespace strata {

class FunctionImpl final : public CoreObject<FunctionImpl, IFunctionInternal, IEvent>
{
public:
    FunctionImpl() = default;
    ReturnValue invoke(const IAny *args, InvokeType type = Immediate) const override;
    void set_invoke_callback(IFunction::CallableFn *fn) override;
    void bind(void* context, IFunctionInternal::BoundFn* fn) override;

    // IEvent
    ReturnValue add_handler(const IFunction::ConstPtr &fn, InvokeType type = Immediate) const override;
    ReturnValue remove_handler(const IFunction::ConstPtr &fn, InvokeType type = Immediate) const override;

private:
    void invoke_handlers(const IAny *args) const;

    IFunction::CallableFn *fn_{};
    void* bound_context_{};
    IFunctionInternal::BoundFn* bound_fn_{};
    mutable std::vector<IFunction::ConstPtr> handlers_;
    mutable std::vector<IFunction::ConstPtr> deferred_handlers_;
};

} // namespace strata

#endif // FUNCTION_H
