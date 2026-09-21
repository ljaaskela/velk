#ifndef VELK_INTF_TASK_POOL_H
#define VELK_INTF_TASK_POOL_H

#include <velk/duration.h>
#include <velk/interface/intf_function.h>
#include <velk/interface/intf_future.h>
#include <velk/interface/types.h>

namespace velk {

namespace ClassId {
/** @brief Task pool that executes tasks on a set of worker threads.
 *  @see velk::ThreadedTaskPool (api/task_pool.h) */
inline constexpr Uid ThreadedTaskPool{"861b4e2e-9109-4316-8bd7-3b333faebd11"};
/** @brief Task pool whose tasks run when its owner calls drain().
 *  @see velk::ManualTaskPool (api/task_pool.h) */
inline constexpr Uid ManualTaskPool{"070dfea3-4dc9-44c2-9501-567dd3f89769"};
} // namespace ClassId

/**
 * @brief Accepts tasks and executes them.
 *
 * The execution model depends on the implementation: IThreadedTaskPool runs tasks
 * on worker threads, IManualTaskPool runs them when its owner calls drain().
 * Tasks are invoked with empty FnArgs. submit() and post() are safe to call from
 * any thread.
 *
 * A shared threaded pool is available through IVelk::task_pool().
 */
class ITaskPool : public Interface<ITaskPool>
{
public:
    /**
     * @brief Submits a task and returns a future for its result.
     *
     * The future resolves with the task's return value (nullptr resolves as void).
     * Continuations added with InvokeType::Auto run immediately if the task runs on
     * the thread that called this function, otherwise during that thread's next
     * instance().update().
     *
     * @return The future, or nullptr if @p task is null.
     */
    virtual IFuture::Ptr submit(const IFunction::ConstPtr& task) = 0;

    /**
     * @brief Posts a fire-and-forget task. No future is allocated.
     * @return Success, or InvalidArgument if @p task is null.
     */
    virtual ReturnValue post(const IFunction::ConstPtr& task) = 0;

    /** @brief Returns the number of queued tasks that have not started yet. */
    virtual size_t pending() const = 0;
};

/**
 * @brief Task pool backed by worker threads.
 *
 * Workers are started on the first submit() or post().
 *
 * Chain: IInterface -> ITaskPool -> IThreadedTaskPool
 */
class IThreadedTaskPool : public Interface<IThreadedTaskPool, ITaskPool>
{
public:
    /**
     * @brief Sets the number of worker threads.
     * @param count Worker count, 0 for the default (hardware concurrency minus one, at least one).
     * @return Success, or Refused if the workers have already been started.
     */
    virtual ReturnValue set_thread_count(uint32_t count) = 0;

    /** @brief Returns the number of worker threads the pool uses. */
    virtual uint32_t thread_count() const = 0;
};

/**
 * @brief Task pool drained explicitly by its owner.
 *
 * Useful for work that must run on a specific thread at a specific point, with a
 * per call limit. For example, GPU uploads posted by loader threads and drained by
 * the renderer each frame within a time budget.
 *
 * Chain: IInterface -> ITaskPool -> IManualTaskPool
 */
class IManualTaskPool : public Interface<IManualTaskPool, ITaskPool>
{
public:
    /**
     * @brief Runs queued tasks on the calling thread, in submission order.
     *
     * The budget is checked between tasks, so a running task is never interrupted
     * and at least one task runs if any are queued. Tasks queued while draining run
     * in the next drain(). Tasks left over stay queued in order.
     *
     * Futures from submit() are owned by the submitting thread. If drain() runs on
     * that thread, Auto continuations run immediately; otherwise they are deferred
     * to that thread's next instance().update().
     *
     * @param max_tasks Maximum number of tasks to run, 0 for no limit.
     * @param budget Time budget for this call, zero for no limit.
     * @return The number of tasks run.
     */
    virtual size_t drain(size_t max_tasks = 0, Duration budget = {}) = 0;
};

} // namespace velk

#endif // VELK_INTF_TASK_POOL_H
