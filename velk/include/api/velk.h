#ifndef API_PLUGIN_H
#define API_PLUGIN_H

#include <interface/intf_velk.h>
#include <velk_export.h>

namespace velk {

/** @brief Returns a reference to the global Velk singleton. */
[[maybe_unused]] VELK_EXPORT IVelk &instance();

} // namespace velk

#endif // API_PLUGIN_H
