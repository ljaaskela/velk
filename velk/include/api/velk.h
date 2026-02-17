#ifndef API_PLUGIN_H
#define API_PLUGIN_H

#include <interface/intf_velk.h>

namespace velk {

/** @brief Returns a reference to the global Velk singleton. */
[[maybe_unused]] IVelk &instance();

} // namespace velk

#endif // API_PLUGIN_H
