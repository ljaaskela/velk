#include "manual_task_pool.h"

#include <velk/api/velk.h>

#include <chrono>

namespace velk::impl {

IFuture::Ptr ManualTaskPool::submit(const IFunction::ConstPtr& task)
{
    if (!task) {
        return nullptr;
    }
    auto future = instance().create_future();
    std::lock_guard lock(mutex_);
    queue_.push_back({task, future});
    return future;
}

ReturnValue ManualTaskPool::post(const IFunction::ConstPtr& task)
{
    if (!task) {
        return ReturnValue::InvalidArgument;
    }
    std::lock_guard lock(mutex_);
    queue_.push_back({task, nullptr});
    return ReturnValue::Success;
}

size_t ManualTaskPool::pending() const
{
    std::lock_guard lock(mutex_);
    return queue_.size();
}

size_t ManualTaskPool::drain(size_t max_tasks, Duration budget)
{
    using Clock = std::chrono::steady_clock;

    size_t limit;
    {
        std::lock_guard lock(mutex_);
        // Only tasks queued before this call; tasks queued while draining wait for the next drain.
        limit = queue_.size();
    }
    if (max_tasks && max_tasks < limit) {
        limit = max_tasks;
    }
    const bool timed = budget.us > 0;
    const auto deadline = Clock::now() + std::chrono::microseconds(budget.us);

    size_t run = 0;
    while (run < limit) {
        if (run && timed && Clock::now() >= deadline) {
            break;
        }
        Task task;
        {
            std::lock_guard lock(mutex_);
            if (queue_.empty()) {
                break;
            }
            task = std::move(queue_.front());
            queue_.pop_front();
        }
        // Immediate: Auto would resolve against the function's owner thread and
        // bounce the task to the owner's deferred queue.
        auto result = task.fn->invoke({}, Immediate);
        if (auto* internal = interface_cast<IFutureInternal>(task.future)) {
            internal->set_result(result.get());
        }
        ++run;
    }
    return run;
}

} // namespace velk::impl
