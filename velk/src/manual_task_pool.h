#ifndef MANUAL_TASK_POOL_H
#define MANUAL_TASK_POOL_H

#include <velk/common.h>
#include <velk/ext/core_object.h>
#include <velk/interface/intf_task_pool.h>

#include <deque>
#include <mutex>

namespace velk::impl {

/**
 * @brief IManualTaskPool implementation.
 *
 * Tasks may be queued from any thread and run on whichever thread calls drain().
 * On destruction, queued tasks are dropped (their futures never resolve).
 */
class ManualTaskPool final : public ext::ObjectCore<ManualTaskPool, IManualTaskPool>
{
public:
    VELK_CLASS_UID(ClassId::ManualTaskPool, "ManualTaskPool");

    ManualTaskPool() = default;

public: // ITaskPool
    IFuture::Ptr submit(const IFunction::ConstPtr& task) override;
    ReturnValue post(const IFunction::ConstPtr& task) override;
    size_t pending() const override;

public: // IManualTaskPool
    size_t drain(size_t max_tasks, Duration budget) override;

private:
    struct Task
    {
        IFunction::ConstPtr fn;
        IFuture::Ptr future; ///< Null for fire-and-forget tasks.
    };

    mutable std::mutex mutex_;
    std::deque<Task> queue_;
};

} // namespace velk::impl

#endif // MANUAL_TASK_POOL_H
