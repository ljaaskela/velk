#include "threaded_task_pool.h"

#include <velk/api/velk.h>

namespace velk::impl {

ThreadedTaskPool::~ThreadedTaskPool()
{
    std::deque<Task> dropped;
    {
        std::lock_guard lock(shared_->mutex);
        shared_->stopping = true;
        dropped.swap(shared_->queue);
    }
    shared_->cv.notify_all();
    auto self = std::this_thread::get_id();
    for (auto& worker : workers_) {
        // A task may release the last reference to its own pool. That worker
        // cannot join itself; it exits its loop once the task returns.
        if (worker.get_id() == self) {
            worker.detach();
        } else {
            worker.join();
        }
    }
}

IFuture::Ptr ThreadedTaskPool::submit(const IFunction::ConstPtr& task)
{
    if (!task) {
        return nullptr;
    }
    auto future = instance().create_future();
    enqueue({task, future});
    return future;
}

ReturnValue ThreadedTaskPool::post(const IFunction::ConstPtr& task)
{
    if (!task) {
        return ReturnValue::InvalidArgument;
    }
    enqueue({task, nullptr});
    return ReturnValue::Success;
}

size_t ThreadedTaskPool::pending() const
{
    std::lock_guard lock(shared_->mutex);
    return shared_->queue.size();
}

ReturnValue ThreadedTaskPool::set_thread_count(uint32_t count)
{
    std::lock_guard lock(shared_->mutex);
    if (!workers_.empty()) {
        return ReturnValue::Refused;
    }
    thread_count_ = count;
    return ReturnValue::Success;
}

uint32_t ThreadedTaskPool::thread_count() const
{
    std::lock_guard lock(shared_->mutex);
    return resolved_thread_count();
}

uint32_t ThreadedTaskPool::resolved_thread_count() const
{
    if (thread_count_) {
        return thread_count_;
    }
    auto hw = std::thread::hardware_concurrency();
    return hw > 1 ? hw - 1 : 1;
}

void ThreadedTaskPool::enqueue(Task task)
{
    {
        std::lock_guard lock(shared_->mutex);
        shared_->queue.push_back(std::move(task));
        if (workers_.empty()) {
            auto count = resolved_thread_count();
            workers_.reserve(count);
            for (uint32_t i = 0; i < count; ++i) {
                workers_.emplace_back([shared = shared_] { worker_loop(shared); });
            }
        }
    }
    shared_->cv.notify_one();
}

void ThreadedTaskPool::worker_loop(const std::shared_ptr<Shared>& shared)
{
    for (;;) {
        Task task;
        {
            std::unique_lock lock(shared->mutex);
            shared->cv.wait(lock, [&] { return shared->stopping || !shared->queue.empty(); });
            if (shared->stopping) {
                return;
            }
            task = std::move(shared->queue.front());
            shared->queue.pop_front();
        }
        // Immediate: Auto would resolve against the function's owner thread and
        // bounce the task back to the owner's deferred queue.
        auto result = task.fn->invoke({}, Immediate);
        if (auto* internal = interface_cast<IFutureInternal>(task.future)) {
            internal->set_result(result.get());
        }
    }
}

} // namespace velk::impl
