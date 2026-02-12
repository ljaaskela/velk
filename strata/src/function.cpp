#include "function.h"
#include <api/strata.h>

namespace strata {

ReturnValue FunctionImpl::invoke(const IAny *args, InvokeType type) const
{
    if (type == Deferred) {
        IStrata::DeferredTask task;
        task.fn = get_self<IFunction>();
        task.args = args ? args->clone() : nullptr;
        instance().queue_deferred_tasks(array_view(&task, 1));
        return ReturnValue::SUCCESS;
    }

    bool has_primary = fn_ || bound_fn_;
    ReturnValue result = ReturnValue::NOTHING_TO_DO;
    if (fn_) {
        result = fn_(args);
    } else if (bound_fn_) {
        result = bound_fn_(bound_context_, args);
    }
    invoke_handlers(args);

    if (has_primary) {
        return result;
    }
    return (handlers_.empty() && deferred_handlers_.empty()) ? ReturnValue::NOTHING_TO_DO : ReturnValue::SUCCESS;
}

void FunctionImpl::invoke_handlers(const IAny *args) const
{
    // Direct handlers
    for (const auto &h : handlers_) {
        h->invoke(args);
    }
    // Deferred handlers
    if (!deferred_handlers_.empty()) {
        std::vector<IStrata::DeferredTask> tasks;
        tasks.reserve(deferred_handlers_.size());
        for (const auto &h : deferred_handlers_) {
            tasks.push_back({h, args ? args->clone() : nullptr});
        }
        instance().queue_deferred_tasks(array_view(&tasks.front(), tasks.size()));
    }
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

ReturnValue FunctionImpl::add_handler(const IFunction::ConstPtr &fn, InvokeType type) const
{
    if (!fn) {
        return ReturnValue::INVALID_ARGUMENT;
    }
    auto &list = (type == Deferred) ? deferred_handlers_ : handlers_;
    for (const auto &h : list) {
        if (h == fn) {
            return ReturnValue::NOTHING_TO_DO;
        }
    }
    list.push_back(fn);
    return ReturnValue::SUCCESS;
}

ReturnValue FunctionImpl::remove_handler(const IFunction::ConstPtr &fn, InvokeType type) const
{
    auto &list = (type == Deferred) ? deferred_handlers_ : handlers_;
    auto pos = 0;
    for (const auto &h : list) {
        if (h == fn) {
            list.erase(list.begin() + pos);
            return ReturnValue::SUCCESS;
        }
        pos++;
    }
    return ReturnValue::NOTHING_TO_DO;
}

} // namespace strata
