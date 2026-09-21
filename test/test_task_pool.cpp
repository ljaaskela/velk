#include <velk/api/callback.h>
#include <velk/api/future.h>
#include <velk/api/task_pool.h>
#include <velk/api/velk.h>
#include <velk/interface/intf_task_pool.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

using namespace velk;

namespace {

/** Calls instance().update() until @p done returns true or the timeout expires. */
template <class Pred>
bool update_until(Pred done, std::chrono::milliseconds timeout = std::chrono::milliseconds(2000))
{
    auto end = std::chrono::steady_clock::now() + timeout;
    while (!done()) {
        if (std::chrono::steady_clock::now() > end) {
            return false;
        }
        instance().update();
        std::this_thread::yield();
    }
    return true;
}

} // namespace

TEST(TaskPool, DefaultPoolIsShared)
{
    auto a = instance().task_pool();
    auto b = instance().task_pool();
    ASSERT_TRUE(a);
    EXPECT_EQ(a.get(), b.get());

    ITaskPool::Ptr c = default_task_pool();
    EXPECT_EQ(a.get(), c.get());
}

TEST(TaskPool, DefaultThreadCount)
{
    auto pool = create_threaded_task_pool();
    ASSERT_TRUE(pool);
    EXPECT_GE(pool.thread_count(), 1u);
}

TEST(TaskPool, PostRunsOnWorkerThread)
{
    auto pool = create_threaded_task_pool(2);
    auto main_id = std::this_thread::get_id();

    std::atomic<bool> done{false};
    std::thread::id worker_id;
    EXPECT_EQ(pool.post([&](FnArgs) -> ReturnValue {
        worker_id = std::this_thread::get_id();
        done = true;
        return ReturnValue::Success;
    }),
              ReturnValue::Success);

    auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(2000);
    while (!done && std::chrono::steady_clock::now() < end) {
        std::this_thread::yield();
    }
    ASSERT_TRUE(done);
    EXPECT_NE(worker_id, main_id);
}

TEST(TaskPool, SubmitTyped)
{
    auto pool = create_threaded_task_pool(2);
    auto future = pool.submit([]() -> int { return 21 * 2; });
    ASSERT_TRUE(future);
    future.wait();
    EXPECT_TRUE(future.is_ready());
    EXPECT_EQ(future.get_result().get_value(), 42);
}

TEST(TaskPool, SubmitVoid)
{
    auto pool = create_threaded_task_pool(1);
    std::atomic<bool> ran{false};
    auto future = pool.submit([&]() { ran = true; });
    ASSERT_TRUE(future);
    future.wait();
    EXPECT_TRUE(ran);
}

TEST(TaskPool, ManyTasksAllComplete)
{
    auto pool = create_threaded_task_pool(4);
    constexpr int count = 1000;
    std::atomic<int> counter{0};
    for (int i = 0; i < count - 1; ++i) {
        pool.post([&](FnArgs) -> ReturnValue {
            ++counter;
            return ReturnValue::Success;
        });
    }
    auto last = pool.submit([&]() { ++counter; });

    auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(5000);
    while (counter < count && std::chrono::steady_clock::now() < end) {
        std::this_thread::yield();
    }
    last.wait();
    EXPECT_EQ(counter.load(), count);
    EXPECT_EQ(pool.pending(), 0u);
}

TEST(TaskPool, AutoContinuationRunsOnMainThreadDuringUpdate)
{
    auto pool = create_threaded_task_pool(1);
    auto main_id = std::this_thread::get_id();

    std::atomic<bool> called{false};
    std::thread::id continuation_id;
    auto task = pool.submit([]() -> int { return 7; });
    auto chained = task.then([&](int value) -> int {
        continuation_id = std::this_thread::get_id();
        called = true;
        return value + 1;
    });

    task.wait();
    // The result was set on a worker, so the Auto continuation is queued, not run.
    EXPECT_FALSE(called);

    ASSERT_TRUE(update_until([&] { return called.load(); }));
    EXPECT_EQ(continuation_id, main_id);
    EXPECT_TRUE(chained.is_ready());
    EXPECT_EQ(chained.get_result().get_value(), 8);
}

TEST(TaskPool, ImmediateContinuationRunsOnWorker)
{
    auto pool = create_threaded_task_pool(1);
    auto main_id = std::this_thread::get_id();

    std::thread::id continuation_id;
    auto task = pool.submit([]() -> int { return 3; });
    auto chained = task.then(
        [&](int value) -> int {
            continuation_id = std::this_thread::get_id();
            return value * 3;
        },
        Immediate);

    chained.wait();
    EXPECT_NE(continuation_id, main_id);
    EXPECT_EQ(chained.get_result().get_value(), 9);
}

TEST(TaskPool, SetThreadCountRefusedAfterStart)
{
    auto pool = create_threaded_task_pool();
    EXPECT_EQ(pool.set_thread_count(3), ReturnValue::Success);
    EXPECT_EQ(pool.thread_count(), 3u);

    pool.submit([]() {}).wait();
    EXPECT_EQ(pool.set_thread_count(5), ReturnValue::Refused);
    EXPECT_EQ(pool.thread_count(), 3u);
}

TEST(TaskPool, DestroyWithQueuedTasksDoesNotHang)
{
    std::atomic<int> counter{0};
    std::atomic<bool> started{false};
    {
        auto pool = create_threaded_task_pool(1);
        pool.post([&](FnArgs) -> ReturnValue {
            started = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            return ReturnValue::Success;
        });
        for (int i = 0; i < 100; ++i) {
            pool.post([&](FnArgs) -> ReturnValue {
                ++counter;
                return ReturnValue::Success;
            });
        }
        while (!started) {
            std::this_thread::yield();
        }
    }
    // The running task finished; the queued ones were dropped.
    EXPECT_LT(counter.load(), 100);
}

TEST(TaskPool, TaskReleasingLastPoolReference)
{
    std::atomic<bool> done{false};
    {
        auto pool = create_threaded_task_pool(1);
        ITaskPool::Ptr keep = pool;
        pool.post([keep, &done](FnArgs) mutable -> ReturnValue {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            keep = nullptr;
            done = true;
            return ReturnValue::Success;
        });
    }
    // The pool is destroyed on its own worker when the task's callback is released.
    auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(2000);
    while (!done && std::chrono::steady_clock::now() < end) {
        std::this_thread::yield();
    }
    EXPECT_TRUE(done);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
}

TEST(TaskPool, NullPoolIsSafe)
{
    ThreadedTaskPool pool(nullptr);
    EXPECT_FALSE(pool);
    EXPECT_EQ(pool.post([](FnArgs) -> ReturnValue { return ReturnValue::Success; }), ReturnValue::Fail);
    EXPECT_FALSE(pool.submit([]() -> int { return 1; }));
    EXPECT_EQ(pool.pending(), 0u);
    EXPECT_EQ(pool.thread_count(), 0u);
    EXPECT_EQ(pool.set_thread_count(2), ReturnValue::Fail);
}

TEST(TaskPool, RawInterface)
{
    auto pool = instance().create<ITaskPool>(ClassId::ThreadedTaskPool);
    ASSERT_TRUE(pool);
    EXPECT_TRUE(interface_cast<IThreadedTaskPool>(pool));
    EXPECT_FALSE(interface_cast<IManualTaskPool>(pool));
    EXPECT_EQ(pool->post(nullptr), ReturnValue::InvalidArgument);
    EXPECT_FALSE(pool->submit(nullptr));

    Callback task([](FnArgs) -> IAny::Ptr { return Any<int>(5).clone(); });
    auto future = pool->submit(task);
    ASSERT_TRUE(future);
    future->wait();
    EXPECT_EQ(Any<const int>(future->get_result()).get_value(), 5);
}

// Manual task pool

TEST(ManualTaskPool, Create)
{
    auto pool = create_manual_task_pool();
    ASSERT_TRUE(pool);
    EXPECT_TRUE(interface_cast<IManualTaskPool>(ITaskPool::Ptr(pool)));
    EXPECT_FALSE(interface_cast<IThreadedTaskPool>(ITaskPool::Ptr(pool)));
    EXPECT_EQ(pool.pending(), 0u);
    EXPECT_EQ(pool.drain(), 0u);
}

TEST(ManualTaskPool, RunsOnlyOnDrainInOrderOnCallingThread)
{
    auto pool = create_manual_task_pool();
    auto main_id = std::this_thread::get_id();

    std::vector<int> order;
    bool same_thread = true;
    for (int i = 0; i < 5; ++i) {
        pool.post([&, i](FnArgs) -> ReturnValue {
            order.push_back(i);
            same_thread &= std::this_thread::get_id() == main_id;
            return ReturnValue::Success;
        });
    }
    EXPECT_TRUE(order.empty());
    EXPECT_EQ(pool.pending(), 5u);

    EXPECT_EQ(pool.drain(), 5u);
    EXPECT_EQ(order, (std::vector<int>{0, 1, 2, 3, 4}));
    EXPECT_TRUE(same_thread);
    EXPECT_EQ(pool.pending(), 0u);
}

TEST(ManualTaskPool, DrainMaxTasks)
{
    auto pool = create_manual_task_pool();
    int count = 0;
    for (int i = 0; i < 10; ++i) {
        pool.post([&](FnArgs) -> ReturnValue {
            ++count;
            return ReturnValue::Success;
        });
    }
    EXPECT_EQ(pool.drain(3), 3u);
    EXPECT_EQ(count, 3);
    EXPECT_EQ(pool.pending(), 7u);

    EXPECT_EQ(pool.drain(), 7u);
    EXPECT_EQ(count, 10);
}

TEST(ManualTaskPool, DrainTimeBudget)
{
    auto pool = create_manual_task_pool();
    for (int i = 0; i < 10; ++i) {
        pool.post([](FnArgs) -> ReturnValue {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            return ReturnValue::Success;
        });
    }
    // Budget smaller than one task: exactly one task still runs.
    EXPECT_EQ(pool.drain(0, Duration::from_microseconds(1)), 1u);
    EXPECT_EQ(pool.pending(), 9u);

    // Budget covering a few tasks stops before the queue is empty.
    auto run = pool.drain(0, Duration::from_milliseconds(25));
    EXPECT_GE(run, 1u);
    EXPECT_LT(run, 9u);
    EXPECT_EQ(pool.pending(), 9u - run);
}

TEST(ManualTaskPool, TasksQueuedDuringDrainRunNextDrain)
{
    auto pool = create_manual_task_pool();
    bool inner = false;
    pool.post([&](FnArgs) -> ReturnValue {
        pool.post([&](FnArgs) -> ReturnValue {
            inner = true;
            return ReturnValue::Success;
        });
        return ReturnValue::Success;
    });

    EXPECT_EQ(pool.drain(), 1u);
    EXPECT_FALSE(inner);
    EXPECT_EQ(pool.pending(), 1u);

    EXPECT_EQ(pool.drain(), 1u);
    EXPECT_TRUE(inner);
}

TEST(ManualTaskPool, SubmitResolvesDuringDrain)
{
    auto pool = create_manual_task_pool();
    auto future = pool.submit([]() -> int { return 5; });
    int received = 0;
    // Auto continuation on the owner thread: runs Immediate during drain.
    auto chained = future.then([&](int v) -> int {
        received = v;
        return v * 2;
    });

    EXPECT_FALSE(future.is_ready());
    EXPECT_EQ(pool.drain(), 1u);
    EXPECT_TRUE(future.is_ready());
    EXPECT_EQ(future.get_result().get_value(), 5);
    EXPECT_EQ(received, 5);
    EXPECT_TRUE(chained.is_ready());
    EXPECT_EQ(chained.get_result().get_value(), 10);
}

TEST(ManualTaskPool, WorkerPostsDrainOnMainThread)
{
    // GPU upload flow: decode on workers, upload on the owning thread.
    // Declared first so it outlives the workers, which are joined when their pool is destroyed.
    auto uploads = create_manual_task_pool();
    auto workers = create_threaded_task_pool(4);
    auto main_id = std::this_thread::get_id();

    constexpr int count = 50;
    std::atomic<int> decoded{0};
    int uploaded = 0;
    bool on_main = true;
    for (int i = 0; i < count; ++i) {
        workers.post([&](FnArgs) -> ReturnValue {
            ++decoded;
            uploads.post([&](FnArgs) -> ReturnValue {
                ++uploaded;
                on_main &= std::this_thread::get_id() == main_id;
                return ReturnValue::Success;
            });
            return ReturnValue::Success;
        });
    }

    auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(5000);
    while (uploaded < count && std::chrono::steady_clock::now() < end) {
        uploads.drain(8);
        std::this_thread::yield();
    }
    EXPECT_EQ(decoded.load(), count);
    EXPECT_EQ(uploaded, count);
    EXPECT_TRUE(on_main);
}

TEST(ManualTaskPool, DestroyWithQueuedTasks)
{
    bool ran = false;
    IFuture::Ptr future;
    {
        auto pool = create_manual_task_pool();
        future = pool.submit([&]() { ran = true; });
        pool.post([&](FnArgs) -> ReturnValue {
            ran = true;
            return ReturnValue::Success;
        });
    }
    EXPECT_FALSE(ran);
    ASSERT_TRUE(future);
    EXPECT_FALSE(future->is_ready());
}

TEST(ManualTaskPool, NullPoolIsSafe)
{
    ManualTaskPool pool(nullptr);
    EXPECT_FALSE(pool);
    EXPECT_EQ(pool.post([](FnArgs) -> ReturnValue { return ReturnValue::Success; }), ReturnValue::Fail);
    EXPECT_EQ(pool.drain(), 0u);
    EXPECT_EQ(pool.drain(1, Duration::from_milliseconds(1)), 0u);
}
