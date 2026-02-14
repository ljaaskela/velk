#ifndef FUNCTION_H
#define FUNCTION_H

#include "strata_impl.h"
#include <common.h>
#include <ext/core_object.h>
#include <interface/intf_event.h>
#include <interface/intf_function.h>
#include <interface/types.h>

namespace strata {

class FunctionImpl final : public ObjectCore<FunctionImpl, IFunctionInternal, IEvent>
{
public:
    FunctionImpl() = default;

public: // IFunction
    ReturnValue invoke(const IAny *args, InvokeType type = Immediate) const override;

public: // IFunctionInternal
    void set_invoke_callback(IFunction::CallableFn *fn) override;
    void bind(void *context, IFunctionInternal::BoundFn *fn) override;

public: // IEvent
    ReturnValue add_handler(const IFunction::ConstPtr &fn, InvokeType type = Immediate) const override;
    ReturnValue remove_handler(const IFunction::ConstPtr &fn) const override;
    bool has_handlers() const override;

private:
    static ReturnValue callback_trampoline(void* ctx, const IAny* args);
    void invoke_handlers(const IAny *args) const;
    array_view<IFunction::ConstPtr> immediate_handlers() const;
    array_view<IFunction::ConstPtr> deferred_handlers() const;

    void* target_context_{};              ///< Context for target_fn_: interface ptr (bind) or CallableFn* (set_invoke_callback).
    IFunctionInternal::BoundFn* target_fn_{};  ///< Primary invoke target. Uses callback_trampoline when set via set_invoke_callback.
    /// Partitioned handler list: [0, deferred_begin_) = immediate, [deferred_begin_, size()) = deferred.
    mutable std::vector<IFunction::ConstPtr> handlers_;
    mutable uint32_t deferred_begin_{};  ///< Index separating immediate handlers from deferred handlers.
};

} // namespace strata

#endif // FUNCTION_H
