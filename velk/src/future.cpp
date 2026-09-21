#include "future.h"

#include <velk/api/callback.h>
#include <velk/api/velk.h>

namespace velk::impl {

bool Future::is_ready() const
{
    return ready_.load(std::memory_order_acquire);
}

void Future::wait() const
{
    if (ready_.load(std::memory_order_acquire)) {
        return;
    }
    std::unique_lock lock(mutex_);
    cv_.wait(lock, [this] { return ready_.load(std::memory_order_relaxed); });
}

const IAny* Future::get_result() const
{
    wait();
    return result_.get();
}

ReturnValue Future::set_result(const IAny* result)
{
    vector<Continuation> continuations;
    {
        std::lock_guard lock(mutex_);
        if (ready_.load(std::memory_order_relaxed)) {
            return ReturnValue::NothingToDo;
        }
        result_ = result ? result->clone() : nullptr;
        ready_.store(true, std::memory_order_release);
        continuations.swap(pending_continuations_);
    }
    cv_.notify_all();

    for (auto& cont : continuations) {
        fire_continuation(cont, result_.get());
    }
    return ReturnValue::Success;
}

void Future::add_continuation(const IFunction::ConstPtr& fn, InvokeType type)
{
    if (!fn) {
        return;
    }

    {
        std::lock_guard lock(mutex_);
        if (!ready_.load(std::memory_order_relaxed)) {
            pending_continuations_.push_back({fn, type});
            return;
        }
    }
    // Already ready — fire now
    fire_continuation({fn, type}, result_.get());
}

void Future::fire_continuation(const Continuation& cont, const IAny* result) const
{
    FnArgs args;
    if (result) {
        args = {&result, 1};
    }

    // Auto is resolved here rather than when the continuation was added, so a
    // result set from another thread lands on the owner thread's update().
    if (resolve_invoke_type(cont.type, get_object_data().owner_thread_id) == Immediate) {
        // The timing is decided; don't let the function re-resolve against its own owner.
        cont.fn->invoke(args, Immediate);
    } else {
        DeferredTask task;
        task.fn = cont.fn;
        task.args = ::velk::make_shared<DeferredArgs>(args);
        instance().queue_deferred_tasks(array_view(&task, 1));
    }
}

IFuture::Ptr Future::then(const IFunction::ConstPtr& fn, InvokeType type)
{
    auto chained = instance().create_future();
    auto* internal = interface_cast<IFutureInternal>(chained);
    Callback wrapper([internal, chained, fn](FnArgs args) -> IAny::Ptr {
        auto result = fn->invoke(args, Immediate);
        if (internal) {
            internal->set_result(result.get());
        }
        return nullptr;
    });
    add_continuation(wrapper, type);
    return chained;
}

} // namespace velk::impl
