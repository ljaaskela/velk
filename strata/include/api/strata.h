#ifndef API_PLUGIN_H
#define API_PLUGIN_H

#include <interface/intf_strata.h>

namespace strata {

/** @brief Returns a reference to the global Strata singleton. */
[[maybe_unused]] IStrata &Strata();

} // namespace strata

#endif // API_PLUGIN_H
