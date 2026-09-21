#include <velk/api/perf.h>
#include <velk/ext/core_object.h>

#include <gtest/gtest.h>
#include <atomic>
#include <thread>
#include <vector>

using namespace velk;

struct PerfRecord
{
    uint64_t key;
    std::string label;
    Duration elapsed;
};

class TestPerfSink : public ext::ObjectCore<TestPerfSink, IPerfSink>
{
public:
    void start_perf(uint64_t, string_view, const char*, uint32_t) override {}

    void end_perf(uint64_t key, string_view label, Duration elapsed) override
    {
        records.push_back({key, std::string(label.data(), label.size()), elapsed});
    }

    void event(PerfEvent) override {}

    std::vector<PerfRecord> records;
};

class PerfTest : public ::testing::Test
{
protected:
    IVelk& velk_ = instance();
    IPerfSink::Ptr sink_ = ext::make_object<TestPerfSink, IPerfSink>();
    TestPerfSink* ts_ = static_cast<TestPerfSink*>(sink_.get());

    void SetUp() override { velk_.perf_log().set_perf_sink(sink_); }

    void TearDown() override
    {
        velk_.perf_log().set_perf_sink({});
        velk_.perf_log().reset_stats();
    }
};

TEST_F(PerfTest, StartEndRecordsTiming)
{
    constexpr uint64_t key = make_hash64("test_region");
    velk_.perf_log().start_perf(key, "test_region");
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    auto elapsed = velk_.perf_log().end_perf(key);

    EXPECT_GT(elapsed.to_microseconds(), 0);
    ASSERT_EQ(1u, ts_->records.size());
    EXPECT_EQ(key, ts_->records[0].key);
    EXPECT_EQ("test_region", ts_->records[0].label);
    EXPECT_GE(ts_->records[0].elapsed.to_milliseconds(), 1);
    EXPECT_EQ(elapsed.to_microseconds(), ts_->records[0].elapsed.to_microseconds());
}

TEST_F(PerfTest, PerfScopeRAII)
{
    {
        PerfScope ps("scoped_region");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    ASSERT_EQ(1u, ts_->records.size());
    EXPECT_EQ(make_hash64("scoped_region"), ts_->records[0].key);
    EXPECT_EQ("scoped_region", ts_->records[0].label);
    EXPECT_GE(ts_->records[0].elapsed.to_microseconds(), 1);
}

TEST_F(PerfTest, PerfScopeElapsed)
{
    PerfScope ps("elapsed_check");
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    auto mid = ps.elapsed();
    EXPECT_GE(mid.to_milliseconds(), 1);
}

TEST_F(PerfTest, GetPerfRunningRegion)
{
    constexpr uint64_t key = make_hash64("running");
    velk_.perf_log().start_perf(key, "running");
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    auto elapsed = velk_.perf_log().get_perf(key);
    EXPECT_GE(elapsed.to_milliseconds(), 1);

    velk_.perf_log().end_perf(key);
}

TEST_F(PerfTest, GetPerfUnknownKeyReturnsZero)
{
    auto elapsed = velk_.perf_log().get_perf(12345);
    EXPECT_EQ(0, elapsed.to_microseconds());
}

TEST_F(PerfTest, NoSinkNoCrash)
{
    velk_.perf_log().set_perf_sink({});
    constexpr uint64_t key = make_hash64("no_sink");
    velk_.perf_log().start_perf(key, "no_sink");
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    auto elapsed = velk_.perf_log().end_perf(key);
    EXPECT_GE(elapsed.to_milliseconds(), 1);
}

TEST_F(PerfTest, SameKeyOnTwoThreadsOverlaps)
{
    // No sink: TestPerfSink's record list is not thread-safe.
    velk_.perf_log().set_perf_sink({});
    constexpr uint64_t key = make_hash64("overlap");

    std::atomic<int> step{0};
    Duration inner{};
    std::thread other([&] {
        while (step.load() < 1) {
            std::this_thread::yield();
        }
        velk_.perf_log().start_perf(key, "overlap");
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        inner = velk_.perf_log().end_perf(key);
        step = 2;
    });

    velk_.perf_log().start_perf(key, "overlap");
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    step = 1;
    while (step.load() < 2) {
        std::this_thread::yield();
    }
    auto outer = velk_.perf_log().end_perf(key);
    other.join();

    // Each thread measured its own scope, and the outer one spans the inner.
    EXPECT_GE(inner.to_milliseconds(), 5);
    EXPECT_GE(outer.to_milliseconds(), 10);
}

TEST_F(PerfTest, EndPerfUnknownKeyReturnsZero)
{
    auto elapsed = velk_.perf_log().end_perf(99999);
    EXPECT_EQ(0, elapsed.to_microseconds());
    EXPECT_EQ(0u, ts_->records.size());
}

TEST_F(PerfTest, RestartSameKeyResetsTimer)
{
    constexpr uint64_t key = make_hash64("restart");
    velk_.perf_log().start_perf(key, "restart");
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Elapsed should be >= 10ms
    auto elapsed = velk_.perf_log().get_perf(key);
    EXPECT_GE(elapsed.to_milliseconds(), 10);

    // Restart with the same key resets the timer
    velk_.perf_log().start_perf(key, "restart");
    elapsed = velk_.perf_log().end_perf(key);

    // Elapsed should be close to zero (much less than 10ms)
    EXPECT_LT(elapsed.to_milliseconds(), 5.f);
    ASSERT_EQ(1u, ts_->records.size());
}

TEST_F(PerfTest, MakeHash64Deterministic)
{
    constexpr auto h1 = make_hash64("hello");
    constexpr auto h2 = make_hash64("hello");
    constexpr auto h3 = make_hash64("world");

    EXPECT_EQ(h1, h2);
    EXPECT_NE(h1, h3);
    EXPECT_NE(h1, 0u);
    EXPECT_NE(h3, 0u);
}

TEST_F(PerfTest, PerfScopeWithPrecomputedKey)
{
    constexpr uint64_t key = make_hash64("precomputed");
    {
        PerfScope ps(key);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    ASSERT_EQ(1u, ts_->records.size());
    EXPECT_EQ(key, ts_->records[0].key);
    EXPECT_GT(ts_->records[0].elapsed.to_microseconds(), 0);
}

TEST_F(PerfTest, StatsAccumulate)
{
    constexpr uint64_t key = make_hash64("stats_test");

    for (int i = 0; i < 3; ++i) {
        velk_.perf_log().start_perf(key, "stats_test");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        velk_.perf_log().end_perf(key);
    }

    auto stats = velk_.perf_log().get_stats();
    ASSERT_EQ(1u, stats.size());
    EXPECT_EQ(key, stats[0].key);
    EXPECT_EQ("stats_test", std::string(stats[0].label.data(), stats[0].label.size()));
    EXPECT_EQ(3u, stats[0].count);
    EXPECT_GE(stats[0].min.to_microseconds(), 0);
    EXPECT_GE(stats[0].max.to_microseconds(), stats[0].min.to_microseconds());
    EXPECT_GE(stats[0].avg.to_microseconds(), stats[0].min.to_microseconds());
    EXPECT_LE(stats[0].avg.to_microseconds(), stats[0].max.to_microseconds());
    EXPECT_GT(stats[0].last.to_microseconds(), 0);
}

TEST_F(PerfTest, StatsMultipleKeys)
{
    constexpr uint64_t k1 = make_hash64("key_a");
    constexpr uint64_t k2 = make_hash64("key_b");

    velk_.perf_log().start_perf(k1, "key_a");
    velk_.perf_log().end_perf(k1);
    velk_.perf_log().start_perf(k2, "key_b");
    velk_.perf_log().end_perf(k2);

    auto stats = velk_.perf_log().get_stats();
    EXPECT_EQ(2u, stats.size());
}

TEST_F(PerfTest, ResetStatsClearsAll)
{
    constexpr uint64_t key = make_hash64("reset_test");
    velk_.perf_log().start_perf(key, "reset_test");
    velk_.perf_log().end_perf(key);

    EXPECT_EQ(1u, velk_.perf_log().get_stats().size());

    velk_.perf_log().reset_stats();
    EXPECT_EQ(0u, velk_.perf_log().get_stats().size());
}
