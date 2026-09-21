#include "perf_log.h"

#include <velk/api/velk.h>
#include <velk/thread.h>

#include <algorithm>
#include <chrono>

namespace velk {

static Duration now()
{
    using clock = std::chrono::high_resolution_clock;
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(clock::now().time_since_epoch()).count();
    return Duration::from_microseconds(us);
}

PerfLog::PerfLog()
{
    // Initially reserve space for 16 parallel perf logs
    perf_entries_.reserve(16);
}

static void compute_percentiles(PerfStats& s)
{
    if (s.sample_count == 0) {
        return;
    }
    Duration sorted[PerfStats::kSampleCapacity];
    for (uint32_t i = 0; i < s.sample_count; i++) {
        sorted[i] = s.samples[i];
    }
    std::sort(sorted, sorted + s.sample_count, [](Duration a, Duration b) {
        return a.to_microseconds() < b.to_microseconds();
    });
    s.median = sorted[s.sample_count / 2];
    s.p95 = sorted[s.sample_count * 95 / 100];
}

void PerfLog::print_stats() const
{
    if (stats_entries_.empty()) {
        return;
    }
    VELK_LOG(I, "Perf stats:");
    for (auto& s : stats_entries_) {
        compute_percentiles(s);
        VELK_LOG(I,
                 "  %-30.*s  med=%7.3fms  p95=%7.3fms  min=%7.3fms  max=%7.3fms  avg=%7.3fms  count=%u",
                 static_cast<int>(s.label.size()),
                 s.label.data(),
                 s.median.to_milliseconds(),
                 s.p95.to_milliseconds(),
                 s.min.to_milliseconds(),
                 s.max.to_milliseconds(),
                 s.avg.to_milliseconds(),
                 s.count);
    }
}

void PerfLog::start_perf(uint64_t key, string_view label, const char* file, uint32_t line)
{
    auto start = now();
    auto thread = current_thread_id();
    std::lock_guard<std::mutex> lock(perf_mutex_);
    if (perf_sink_) {
        perf_sink_->start_perf(key, label, file, line);
    }
    for (auto& e : perf_entries_) {
        if (e.key == key && e.thread == thread) {
            // Existing key, restart measurement
            e.label = label;
            e.start = start;
            return;
        }
    }
    // New key
    PerfEntry e;
    e.key = key;
    e.thread = thread;
    e.label = label;
    e.start = start;
    perf_entries_.emplace_back(std::move(e));
}

Duration PerfLog::end_perf(uint64_t key)
{
    auto thread = current_thread_id();
    std::lock_guard<std::mutex> lock(perf_mutex_);
    for (auto e = perf_entries_.begin(); e != perf_entries_.end(); e++) {
        if (e->key == key && e->thread == thread) {
            auto elapsed = now() - e->start;
            auto label = e->label;
            if (stats_enabled_) {
                // Accumulate stats
                accumulate(key, label, elapsed);
            }
            if (perf_sink_) {
                perf_sink_->end_perf(key, label, elapsed);
            }
            // Erase from vector
            perf_entries_.erase(e);
            return elapsed;
        }
    }
    VELK_LOG(W, "Invalid perf log key: %zu", key);
    return {};
}

void PerfLog::add_sample(PerfStats& s, Duration elapsed)
{
    s.samples[s.sample_pos] = elapsed;
    s.sample_pos = (s.sample_pos + 1) % PerfStats::kSampleCapacity;
    if (s.sample_count < PerfStats::kSampleCapacity) {
        s.sample_count++;
    }
}

void PerfLog::accumulate(uint64_t key, string_view label, Duration elapsed)
{
    for (auto& s : stats_entries_) {
        if (s.key == key) {
            s.last = elapsed;
            if (elapsed < s.min) s.min = elapsed;
            if (elapsed > s.max) s.max = elapsed;
            s.total += elapsed;
            s.count++;
            s.avg = Duration::from_microseconds(s.total.to_microseconds() / s.count);
            add_sample(s, elapsed);
            return;
        }
    }
    PerfStats s;
    s.key = key;
    s.label = label;
    s.last = elapsed;
    s.min = elapsed;
    s.max = elapsed;
    s.total = elapsed;
    s.avg = elapsed;
    s.count = 1;
    add_sample(s, elapsed);
    stats_entries_.emplace_back(std::move(s));
}

Duration PerfLog::get_perf(uint64_t key) const
{
    auto thread = current_thread_id();
    std::lock_guard<std::mutex> lock(perf_mutex_);
    for (auto& e : perf_entries_) {
        if (e.key == key && e.thread == thread) {
            return now() - e.start;
        }
    }
    VELK_LOG(W, "Invalid perf log key: %zu", key);
    return {};
}

void PerfLog::set_perf_sink(const IPerfSink::Ptr& sink)
{
    std::lock_guard<std::mutex> lock(perf_mutex_);
    perf_sink_ = sink;
}

vector<PerfStats> PerfLog::get_stats() const
{
    std::lock_guard<std::mutex> lock(perf_mutex_);
    auto result = stats_entries_;
    for (auto& s : result) {
        compute_percentiles(s);
    }
    return result;
}

void PerfLog::reset_stats()
{
    std::lock_guard<std::mutex> lock(perf_mutex_);
    stats_entries_.clear();
}

void PerfLog::set_stats_enabled(bool enabled)
{
    std::lock_guard<std::mutex> lock(perf_mutex_);
    stats_enabled_ = enabled;
}

void PerfLog::event(PerfEvent type)
{
    std::lock_guard<std::mutex> lock(perf_mutex_);
    if (perf_sink_) {
        perf_sink_->event(type);
    }
}

} // namespace velk
