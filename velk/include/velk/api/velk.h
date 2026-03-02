#ifndef VELK_API_VELK_H
#define VELK_API_VELK_H

#include <velk/interface/intf_velk.h>
#include <velk/velk_export.h>

namespace velk {

/** @brief Returns a reference to the global Velk singleton. */
[[maybe_unused]] VELK_EXPORT IVelk& instance();

/**
 * @brief Typed wrapper for get_or_load_plugin from instance().plugin_registry().
 * @see IPluginRegistry::get_or_load_plugin
 */
template <class T>
T* get_or_load_plugin(Uid pluginId)
{
    auto& reg = instance().plugin_registry();
    return interface_cast<T>(reg.get_or_load_plugin(pluginId));
}

} // namespace velk

#define _VELK_LOG_D ::velk::LogLevel::Debug
#define _VELK_LOG_I ::velk::LogLevel::Info
#define _VELK_LOG_W ::velk::LogLevel::Warning
#define _VELK_LOG_E ::velk::LogLevel::Error
#define _VELK_LOG_Debug ::velk::LogLevel::Debug
#define _VELK_LOG_Info ::velk::LogLevel::Info
#define _VELK_LOG_Warning ::velk::LogLevel::Warning
#define _VELK_LOG_Error ::velk::LogLevel::Error

/**
 * @def VELK_LOG(level, fmt, ...)
 * @brief Emits a log message through the global velk instance.
 *
 * Accepts both short (D, I, W, E) and long (Debug, Info, Warning, Error)
 * level names. Messages below the current threshold are discarded without
 * formatting.
 *
 * @code
 * VELK_LOG(W, "connection lost: %s", reason);
 * VELK_LOG(Debug, "tick %d", frame);
 * @endcode
 */
#define VELK_LOG(level, fmt, ...) \
    ::velk::detail::velk_log(     \
        ::velk::instance().log(), _VELK_LOG_##level, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#endif // VELK_API_VELK_H
