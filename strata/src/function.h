#ifndef FUNCTION_H
#define FUNCTION_H

#include "strata_impl.h"
#include <common.h>
#include <ext/core_object.h>
#include <interface/intf_event.h>
#include <interface/intf_function.h>
#include <interface/types.h>

namespace strata {

class FunctionImpl final : public ext::ObjectCore<FunctionImpl, IFunctionInternal, IEvent>
{
public:
    FunctionImpl() = default;
    ~FunctionImpl();

public: // IFunction
    IAny::Ptr invoke(FnArgs args, InvokeType type = Immediate) const override;

public: // IFunctionInternal
    void set_invoke_callback(IFunction::CallableFn *fn) override;
    void bind(void *context, IFunctionInternal::BoundFn *fn) override;
    void set_owned_callback(void* context, BoundFn* fn, ContextDeleter* deleter) override;

public: // IEvent
    ReturnValue add_handler(const IFunction::ConstPtr &fn, InvokeType type = Immediate) const override;
    ReturnValue remove_handler(const IFunction::ConstPtr &fn) const override;
    bool has_handlers() const override;

private:
    static IAny::Ptr callback_trampoline(void* ctx, FnArgs args);
    void invoke_handlers(FnArgs args) const;
    array_view<IFunction::ConstPtr> immediate_handlers() const;
    array_view<IFunction::ConstPtr> deferred_handlers() const;

    void release_owned_context();

    void* target_context_{};              ///< Context for target_fn_: interface ptr (bind) or CallableFn* (set_invoke_callback).
    IFunctionInternal::BoundFn* target_fn_{};  ///< Primary invoke target. Uses callback_trampoline when set via set_invoke_callback.
    void* owned_context_{};               ///< Heap-allocated callable context (owned).
    IFunctionInternal::ContextDeleter* context_deleter_{};  ///< Deleter for owned_context_.
    /// Partitioned handler list: [0, deferred_begin_) = immediate, [deferred_begin_, size()) = deferred.
    mutable std::vector<IFunction::ConstPtr> handlers_;
    mutable uint32_t deferred_begin_{};  ///< Index separating immediate handlers from deferred handlers.
};

} // namespace strata

#endif // FUNCTION_H
