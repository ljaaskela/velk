#ifndef VELK_INTF_LOG_H
#define VELK_INTF_LOG_H

#include <velk/interface/intf_interface.h>
#include <velk/velk_export.h>

namespace velk {

/** @brief Severity levels for log messages (ordered lowest to highest). */
enum class LogLevel : int32_t {
    Debug   = 0, ///< Verbose diagnostic output.
    Info    = 1, ///< General informational messages.
    Warning = 2, ///< Potential issues that may require attention.
    Error   = 3, ///< Failures that affect correctness.
};

/**
 * @brief Sink interface for receiving log messages.
 *
 * Implement this interface and pass it to ILog::set_sink() to redirect
 * log output. The default sink writes to stderr.
 */
class ILogSink : public Interface<ILogSink>
{
public:
    /** @brief Called by the log system to deliver a formatted message. */
    virtual void write(LogLevel level, const char* file, int line,
                       const char* message) = 0;
};

/**
 * @brief Logging interface exposed by the velk instance.
 *
 * Retrieved via instance().log(). Supports configurable log level
 * and pluggable sink.
 */
class ILog : public Interface<ILog>
{
public:
    /** @brief Sets the log sink. Pass an empty pointer to restore the default stderr sink. */
    virtual void set_sink(const ILogSink::Ptr& sink) = 0;
    /** @brief Sets the minimum log level. Messages below this level are discarded. */
    virtual void set_level(LogLevel level) = 0;
    /** @brief Returns the current minimum log level. */
    virtual LogLevel get_level() const = 0;
    /**
     * @brief Dispatches a pre-formatted log message to the active sink.
     *
     * Callers should generally use the VELK_LOG macro or the velk_log()
     * free function instead of calling this directly.
     */
    virtual void dispatch(LogLevel level, const char* file, int line,
                          const char* message) = 0;
};

namespace detail {

/**
 * @brief Formats and dispatches a log message through @p log.
 * @note Usually not called directly; use VELK_LOG from api/velk.h instead.
 */
VELK_EXPORT void velk_log(ILog& log, LogLevel level,
                           const char* file, int line,
                           const char* fmt, ...);
}

} // namespace velk

#endif // VELK_INTF_LOG_H
