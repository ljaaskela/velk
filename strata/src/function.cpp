#include "function.h"
#include <api/strata.h>

namespace strata {

FunctionImpl::~FunctionImpl()
{
    release_owned_context();
}

void FunctionImpl::release_owned_context()
{
    if (context_deleter_ && owned_context_) {
        context_deleter_(owned_context_);
    }
    owned_context_ = nullptr;
    context_deleter_ = nullptr;
}

ReturnValue FunctionImpl::callback_trampoline(void* ctx, FnArgs args)
{
    return reinterpret_cast<IFunction::CallableFn*>(ctx)(args);
}

ReturnValue FunctionImpl::invoke(FnArgs args, InvokeType type) const
{
    if (type == Deferred) {
        IStrata::DeferredTask task;
        task.fn = get_self<IFunction>();
        task.args = std::make_shared<IStrata::DeferredArgs>(args);
        instance().queue_deferred_tasks(array_view(&task, 1));
        return ReturnValue::SUCCESS;
    }

    ReturnValue result = ReturnValue::NOTHING_TO_DO;
    if (target_fn_) {
        result = target_fn_(target_context_, args);
    }
    invoke_handlers(args);

    if (target_fn_) {
        return result;
    }
    return handlers_.empty() ? ReturnValue::NOTHING_TO_DO : ReturnValue::SUCCESS;
}

array_view<IFunction::ConstPtr> FunctionImpl::immediate_handlers() const
{
    return {handlers_.data(), deferred_begin_};
}

array_view<IFunction::ConstPtr> FunctionImpl::deferred_handlers() const
{
    return {handlers_.data() + deferred_begin_, handlers_.size() - deferred_begin_};
}

void FunctionImpl::invoke_handlers(FnArgs args) const
{
    for (const auto &h : immediate_handlers()) {
        h->invoke(args);
    }
    auto deferred = deferred_handlers();
    if (deferred.empty()) {
        return;
    }
    // Clone args once, share ownership across all deferred tasks
    auto clonedArgs = std::make_shared<IStrata::DeferredArgs>(args);

    std::vector<IStrata::DeferredTask> tasks;
    tasks.reserve(deferred.size());
    for (const auto &h : deferred) {
        tasks.push_back({h, clonedArgs});
    }

    // Queue N tasks to instance for exececution at next instance().update().
    instance().queue_deferred_tasks(array_view(tasks.data(), tasks.size()));
}

void FunctionImpl::set_invoke_callback(IFunction::CallableFn *fn)
{
    release_owned_context();
    target_context_ = reinterpret_cast<void*>(fn);
    target_fn_ = fn ? &callback_trampoline : nullptr;
}

void FunctionImpl::bind(void* context, IFunctionInternal::BoundFn* fn)
{
    release_owned_context();
    target_context_ = context;
    target_fn_ = fn;
}

void FunctionImpl::set_owned_callback(void* context, BoundFn* fn, ContextDeleter* deleter)
{
    release_owned_context();
    owned_context_ = context;
    context_deleter_ = deleter;
    target_context_ = context;
    target_fn_ = fn;
}

ReturnValue FunctionImpl::add_handler(const IFunction::ConstPtr &fn, InvokeType type) const
{
    if (!fn) {
        return ReturnValue::INVALID_ARGUMENT;
    }
    for (const auto &h : handlers_) {
        if (h == fn) return ReturnValue::NOTHING_TO_DO;
    }
    if (type == Immediate) {
        handlers_.insert(handlers_.begin() + deferred_begin_, fn);
        ++deferred_begin_;
    } else {
        handlers_.push_back(fn);
    }
    return ReturnValue::SUCCESS;
}

ReturnValue FunctionImpl::remove_handler(const IFunction::ConstPtr &fn) const
{
    for (size_t i = 0; i < handlers_.size(); ++i) {
        if (handlers_[i] == fn) {
            if (i < deferred_begin_) --deferred_begin_;
            handlers_.erase(handlers_.begin() + i);
            return ReturnValue::SUCCESS;
        }
    }
    return ReturnValue::NOTHING_TO_DO;
}

bool FunctionImpl::has_handlers() const
{
    return !handlers_.empty();
}

} // namespace strata
