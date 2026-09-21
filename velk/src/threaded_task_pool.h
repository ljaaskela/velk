#ifndef THREADED_TASK_POOL_H
#define THREADED_TASK_POOL_H

#include <velk/common.h>
#include <velk/ext/core_object.h>
#include <velk/interface/intf_task_pool.h>
#include <velk/vector.h>

#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>

namespace velk::impl {

/**
 * @brief IThreadedTaskPool implementation backed by worker threads.
 *
 * Workers are started on the first submit or post. On destruction, tasks that have not
 * started are dropped (their futures never resolve) and running tasks are
 * allowed to finish before the workers are joined.
 */
class ThreadedTaskPool final : public ext::ObjectCore<ThreadedTaskPool, IThreadedTaskPool>
{
public:
    VELK_CLASS_UID(ClassId::ThreadedTaskPool, "ThreadedTaskPool");

    ThreadedTaskPool() = default;
    ~ThreadedTaskPool() override;

public: // ITaskPool
    IFuture::Ptr submit(const IFunction::ConstPtr& task) override;
    ReturnValue post(const IFunction::ConstPtr& task) override;
    size_t pending() const override;

public: // IThreadedTaskPool
    ReturnValue set_thread_count(uint32_t count) override;
    uint32_t thread_count() const override;

private:
    struct Task
    {
        IFunction::ConstPtr fn;
        IFuture::Ptr future; ///< Null for fire-and-forget tasks.
    };

    /**
     * @brief Queue state shared with the workers.
     *
     * Owned jointly by the pool and its workers, so a worker whose task released
     * the last reference to the pool can still exit its loop safely.
     */
    struct Shared
    {
        std::mutex mutex;
        std::condition_variable cv;
        std::deque<Task> queue;
        bool stopping{false};
    };

    /** @brief Queues @p task and starts the workers if needed. */
    void enqueue(Task task);
    /** @brief Returns the configured thread count, or the default if unset. Caller holds the mutex. */
    uint32_t resolved_thread_count() const;
    /** @brief Worker thread main loop. */
    static void worker_loop(const std::shared_ptr<Shared>& shared);

    std::shared_ptr<Shared> shared_{std::make_shared<Shared>()};
    vector<std::thread> workers_; ///< Guarded by shared_->mutex.
    uint32_t thread_count_{0};    ///< 0 = default. Guarded by shared_->mutex.
};

} // namespace velk::impl

#endif // THREADED_TASK_POOL_H
