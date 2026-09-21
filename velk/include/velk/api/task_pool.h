#ifndef VELK_API_TASK_POOL_H
#define VELK_API_TASK_POOL_H

#include <velk/api/callback.h>
#include <velk/api/future.h>
#include <velk/api/velk.h>
#include <velk/interface/intf_task_pool.h>

namespace velk {

/**
 * @brief Convenience wrapper around ITaskPool.
 *
 * Accepts the same callables as Callback (FnArgs lambdas, typed lambdas, function
 * pointers). Tasks are invoked with no arguments.
 *
 *   auto pool = default_task_pool();
 *   pool.submit([]() -> int { return load(); })
 *       .then([](int value) { use(value); }); // runs in the next instance().update()
 *
 * All operations are null-safe.
 */
class TaskPool
{
public:
    /** @brief Wraps an existing ITaskPool. */
    explicit TaskPool(ITaskPool::Ptr pool) : pool_(std::move(pool)) {}

    /** @brief Returns true if the underlying ITaskPool is valid. */
    operator bool() const { return pool_.operator bool(); }

    /**
     * @brief Submits a task and returns a Future for its return value.
     *
     * Callables returning void or ReturnValue produce Future<void>. Continuations
     * added with the default InvokeType::Auto run immediately if the task runs on
     * this thread, otherwise in this thread's next instance().update().
     *
     * @param callable A callable compatible with Callback.
     * @return A Future that resolves when the task completes. Invalid if the pool is invalid.
     */
    template <class F>
    auto submit(F&& callable)
    {
        using FR = detail::future_return_t<std::decay_t<F>>;
        if (!pool_) {
            return Future<FR>(nullptr);
        }
        return Future<FR>(pool_->submit(Callback(std::forward<F>(callable))));
    }

    /**
     * @brief Posts a fire-and-forget task. No future is allocated.
     * @param callable A callable compatible with Callback.
     * @return Success, or Fail if the pool is invalid.
     */
    template <class F>
    ReturnValue post(F&& callable)
    {
        return pool_ ? pool_->post(Callback(std::forward<F>(callable))) : ReturnValue::Fail;
    }

    /** @brief Returns the number of queued tasks that have not started yet. */
    size_t pending() const { return pool_ ? pool_->pending() : 0; }

    /** @brief Returns the underlying ITaskPool. */
    ITaskPool& raw() { return *pool_; }
    /** @brief Returns the underlying ITaskPool (const). */
    const ITaskPool& raw() const { return *pool_; }

    /** @brief Implicit conversion to ITaskPool::Ptr. */
    operator ITaskPool::Ptr() const { return pool_; }

protected:
    ITaskPool::Ptr pool_;
};

/** @brief TaskPool wrapper for IThreadedTaskPool, adding worker thread configuration. */
class ThreadedTaskPool : public TaskPool
{
public:
    /** @brief Wraps an existing IThreadedTaskPool. */
    explicit ThreadedTaskPool(IThreadedTaskPool::Ptr pool) : TaskPool(std::move(pool)) {}

    /**
     * @brief Sets the number of worker threads. Only valid before the first submit or post.
     * @param count Worker count, 0 for the default.
     * @return Success, Refused if the workers have already started, or Fail if the pool is invalid.
     */
    ReturnValue set_thread_count(uint32_t count)
    {
        auto* pool = interface_cast<IThreadedTaskPool>(pool_);
        return pool ? pool->set_thread_count(count) : ReturnValue::Fail;
    }

    /** @brief Returns the number of worker threads the pool uses. */
    uint32_t thread_count() const
    {
        auto* pool = interface_cast<IThreadedTaskPool>(pool_);
        return pool ? pool->thread_count() : 0;
    }
};

/**
 * @brief TaskPool wrapper for IManualTaskPool, adding drain().
 *
 *   auto uploads = create_manual_task_pool();
 *   uploads.post([] { upload(); });                      // from any thread
 *   uploads.drain(0, Duration::from_milliseconds(2));    // once per frame
 */
class ManualTaskPool : public TaskPool
{
public:
    /** @brief Wraps an existing IManualTaskPool. */
    explicit ManualTaskPool(IManualTaskPool::Ptr pool) : TaskPool(std::move(pool)) {}

    /**
     * @brief Runs queued tasks on the calling thread.
     * @param max_tasks Maximum number of tasks to run, 0 for no limit.
     * @param budget Time budget for this call, zero for no limit.
     * @return The number of tasks run. 0 if the pool is invalid.
     * @see IManualTaskPool::drain
     */
    size_t drain(size_t max_tasks = 0, Duration budget = {})
    {
        auto* pool = interface_cast<IManualTaskPool>(pool_);
        return pool ? pool->drain(max_tasks, budget) : 0;
    }
};

/** @brief Returns the shared threaded pool from instance().task_pool(). */
inline TaskPool default_task_pool()
{
    return TaskPool(instance().task_pool());
}

/**
 * @brief Creates a new, independent threaded task pool.
 * @param thread_count Worker count, 0 for the default.
 */
inline ThreadedTaskPool create_threaded_task_pool(uint32_t thread_count = 0)
{
    ThreadedTaskPool pool(instance().create<IThreadedTaskPool>(ClassId::ThreadedTaskPool));
    pool.set_thread_count(thread_count);
    return pool;
}

/** @brief Creates a new manual task pool. */
inline ManualTaskPool create_manual_task_pool()
{
    return ManualTaskPool(instance().create<IManualTaskPool>(ClassId::ManualTaskPool));
}

} // namespace velk

#endif // VELK_API_TASK_POOL_H
