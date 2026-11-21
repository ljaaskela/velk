#ifndef VELK_INTF_LOG_H
#define VELK_INTF_LOG_H

#include <velk/interface/intf_interface.h>
#include <velk/string.h>
#include <velk/string_view.h>
#include <velk/velk_export.h>

#include <type_traits>

namespace velk {

/** @brief Severity levels for log messages (ordered lowest to highest). */
enum class LogLevel : int32_t
{
    Debug = 0,   ///< Verbose diagnostic output.
    Info = 1,    ///< General informational messages.
    Warning = 2, ///< Potential issues that may require attention.
    Error = 3,   ///< Failures that affect correctness.
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
    virtual void write(LogLevel level, const char* file, int line, const char* message) = 0;
};

/**
 * @brief Logging interface exposed by the velk instance.
 *
 * Callers should not usually need to handle this interface but rather
 * use the VELK_LOG macro from api/velk.h instead.
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
     * Callers should generally use the VELK_LOG macro instead of calling this directly.
     */
    virtual void dispatch(LogLevel level, const char* file, int line, const char* message) = 0;
};

namespace detail {

/**
 * @brief Formats and dispatches a log message through @p log.
 * @note Usually not called directly; use VELK_LOG from api/velk.h instead.
 */
VELK_EXPORT void velk_log(ILog& log, LogLevel level, const char* file, int line, const char* fmt, ...);

/// A string argument copied so that it ends in a null character, for `%s`.
class LogString
{
public:
    explicit LogString(string_view s) : s_(s) {}
    const char* get() const { return s_.c_str(); }

private:
    string s_;
};

/// A number, enum or pointer argument, passed on unchanged.
template <class T>
struct LogValue
{
    T value;
    T get() const { return value; }
};

inline LogString log_arg(string_view s) { return LogString(s); }
inline LogString log_arg(const string& s) { return LogString(s); }

template <class T>
LogValue<std::decay_t<const T>> log_arg(const T& v)
{
    using D = std::decay_t<const T>;
    static_assert(std::is_scalar_v<D>,
                  "VELK_LOG takes numbers, enums, pointers, string and string_view; "
                  "pass other strings as const char*");
    return {v};
}

/**
 * @brief Formats and dispatches a log message, with `string` and `string_view`
 *        arguments printed by `%s`.
 * @note Usually not called directly; use VELK_LOG from api/velk.h instead.
 */
template <class... Args>
void log_message(ILog& log, LogLevel level, const char* file, int line, const char* fmt, const Args&... args)
{
    velk_log(log, level, file, line, fmt, log_arg(args).get()...);
}

} // namespace detail

} // namespace velk

#endif // VELK_INTF_LOG_H
