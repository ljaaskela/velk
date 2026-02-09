#ifndef API_PLUGIN_H
#define API_PLUGIN_H

#include <interface/intf_registry.h>

/** @brief Returns a reference to the global LTK registry singleton. */
[[maybe_unused]] IRegistry &GetRegistry();

#endif // API_PLUGIN_H
