#include <gtest/gtest.h>

#include <velk/api/velk.h>
#include <velk/ext/core_object.h>

#include <string>
#include <vector>

using namespace velk;

struct LogEntry {
    LogLevel level;
    std::string file;
    int line;
    std::string message;
};

class TestSink : public ext::ObjectCore<TestSink, ILogSink>
{
public:
    void write(LogLevel level, const char* file, int line,
               const char* message) override
    {
        entries.push_back({level, file, line, message});
    }

    std::vector<LogEntry> entries;
};

class LogTest : public ::testing::Test
{
protected:
    IVelk& velk_ = instance();
    ILogSink::Ptr sink_ = ext::make_object<TestSink, ILogSink>();
    TestSink* ts_ = static_cast<TestSink*>(sink_.get());

    void SetUp() override
    {
        velk_.log().set_sink(sink_);
        velk_.log().set_level(LogLevel::Info);
    }

    void TearDown() override
    {
        velk_.log().set_sink({});
        velk_.log().set_level(LogLevel::Info);
    }
};

TEST_F(LogTest, DefaultLevelIsInfo)
{
    EXPECT_EQ(LogLevel::Info, velk_.log().get_level());
}

TEST_F(LogTest, SetAndGetLevel)
{
    velk_.log().set_level(LogLevel::Debug);
    EXPECT_EQ(LogLevel::Debug, velk_.log().get_level());

    velk_.log().set_level(LogLevel::Error);
    EXPECT_EQ(LogLevel::Error, velk_.log().get_level());
}

TEST_F(LogTest, CustomSinkReceivesMessages)
{
    VELK_LOG(I, "hello %d", 42);
    ASSERT_EQ(1u, ts_->entries.size());
    EXPECT_EQ(LogLevel::Info, ts_->entries[0].level);
    EXPECT_EQ("hello 42", ts_->entries[0].message);
    EXPECT_NE(0, ts_->entries[0].line);
}

TEST_F(LogTest, MessagesBelowThresholdDiscarded)
{
    velk_.log().set_level(LogLevel::Warning);

    VELK_LOG(D, "debug msg");
    VELK_LOG(I, "info msg");
    EXPECT_EQ(0u, ts_->entries.size());

    VELK_LOG(W, "warn msg");
    EXPECT_EQ(1u, ts_->entries.size());
    EXPECT_EQ(LogLevel::Warning, ts_->entries[0].level);
}

TEST_F(LogTest, ShortAndLongLevelForms)
{
    VELK_LOG(W, "short warning");
    VELK_LOG(Warning, "long warning");
    ASSERT_EQ(2u, ts_->entries.size());
    EXPECT_EQ(LogLevel::Warning, ts_->entries[0].level);
    EXPECT_EQ(LogLevel::Warning, ts_->entries[1].level);
    EXPECT_EQ("short warning", ts_->entries[0].message);
    EXPECT_EQ("long warning", ts_->entries[1].message);
}

TEST_F(LogTest, AllLevelShortForms)
{
    velk_.log().set_level(LogLevel::Debug);
    VELK_LOG(D, "d");
    VELK_LOG(I, "i");
    VELK_LOG(W, "w");
    VELK_LOG(E, "e");
    ASSERT_EQ(4u, ts_->entries.size());
    EXPECT_EQ(LogLevel::Debug,   ts_->entries[0].level);
    EXPECT_EQ(LogLevel::Info,    ts_->entries[1].level);
    EXPECT_EQ(LogLevel::Warning, ts_->entries[2].level);
    EXPECT_EQ(LogLevel::Error,   ts_->entries[3].level);
}

TEST_F(LogTest, NullSinkRestoresDefault)
{
    velk_.log().set_sink({});
    // Should not crash; writes to stderr
    VELK_LOG(E, "stderr test");
}

TEST_F(LogTest, FormatStringWithNoArgs)
{
    VELK_LOG(I, "no args here");
    ASSERT_EQ(1u, ts_->entries.size());
    EXPECT_EQ("no args here", ts_->entries[0].message);
}

TEST_F(LogTest, DispatchDirectly)
{
    velk_.log().dispatch(LogLevel::Error, "test.cpp", 99, "direct dispatch");
    ASSERT_EQ(1u, ts_->entries.size());
    EXPECT_EQ(LogLevel::Error, ts_->entries[0].level);
    EXPECT_EQ("test.cpp", ts_->entries[0].file);
    EXPECT_EQ(99, ts_->entries[0].line);
    EXPECT_EQ("direct dispatch", ts_->entries[0].message);
}
