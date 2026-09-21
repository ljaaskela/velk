#ifndef VELK_PERF_LOG_H
#define VELK_PERF_LOG_H

#include <velk/ext/interface_dispatch.h>
#include <velk/interface/intf_perf_log.h>
#include <velk/string.h>
#include <velk/vector.h>

#include <mutex>

namespace velk {

/**
 * @brief Concrete implementation of IPerfLog.
 *
 * Owned as a flat member of VelkInstance. Collects scoped timing
 * measurements and dispatches results to an optional IPerfSink.
 */
class PerfLog final : public ext::InterfaceDispatch<IPerfLog>
{
public:
    PerfLog();

    /** @brief Prints accumulated stats via VELK_LOG. */
    void print_stats() const;

    void start_perf(uint64_t key, string_view label, const char* file, uint32_t line) override;
    Duration end_perf(uint64_t key) override;
    Duration get_perf(uint64_t key) const override;
    void set_perf_sink(const IPerfSink::Ptr& sink) override;
    vector<PerfStats> get_stats() const override;
    void reset_stats() override;
    void set_stats_enabled(bool enabled) override;
    void event(PerfEvent type) override;

private:
    /// An open measurement. Keyed by label key and thread, so the same scope
    /// can be open on several threads at once (e.g. task pool workers).
    struct PerfEntry
    {
        uint64_t key = 0;
        uint32_t thread = 0;
        string label;
        Duration start;
    };

    void accumulate(uint64_t key, string_view label, Duration elapsed);
    static void add_sample(PerfStats& s, Duration elapsed);

    mutable vector<PerfEntry> perf_entries_;
    mutable vector<PerfStats> stats_entries_;
    mutable std::mutex perf_mutex_;
    IPerfSink::Ptr perf_sink_;
    bool stats_enabled_{true};
};

} // namespace velk

#endif // VELK_PERF_LOG_H
