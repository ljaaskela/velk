#ifndef VELK_PLUGINS_DATA_PLUGIN_H
#define VELK_PLUGINS_DATA_PLUGIN_H

#include <velk/common.h>

namespace velk {

namespace ClassId {
/** @brief An ordered list of objects (IObjectList). */
inline constexpr Uid ObjectList{"2d1f6a3e-7c42-4b8e-9a15-6e0b3c7d4f21"};
/** @brief An item source over an IHierarchy (IHierarchySource). */
inline constexpr Uid HierarchySource{"8b4e2c71-3f9a-4d06-b5e8-1a7c9d2e6f43"};
} // namespace ClassId

namespace PluginId {
/** @brief Data sources and containers for views (velk_data). */
inline constexpr Uid DataPlugin{"5c7a9e13-2b4d-4f68-8e1a-3d6b0f9c2a57"};
} // namespace PluginId

} // namespace velk

#endif // VELK_PLUGINS_DATA_PLUGIN_H
