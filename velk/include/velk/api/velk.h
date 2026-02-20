#ifndef VELK_API_VELK_H
#define VELK_API_VELK_H

#include <velk/interface/intf_velk.h>
#include <velk/velk_export.h>

namespace velk {

/** @brief Returns a reference to the global Velk singleton. */
[[maybe_unused]] VELK_EXPORT IVelk &instance();

} // namespace velk

#endif // VELK_API_VELK_H
