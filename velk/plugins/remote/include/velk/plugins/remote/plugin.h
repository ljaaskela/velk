#ifndef VELK_PLUGINS_REMOTE_PLUGIN_H
#define VELK_PLUGINS_REMOTE_PLUGIN_H

#include <velk/common.h>

namespace velk {

namespace ClassId {
/** @brief Serves a program's objects to remote clients. */
inline constexpr Uid RemoteServer{"c85c7404-21bc-4d0c-9848-5a5487b4f0e8"};
/** @brief Reads and edits the objects of a program that runs a RemoteServer. */
inline constexpr Uid RemoteClient{"537d3c57-4490-4c7e-85fc-b9c5b4a574cb"};
} // namespace ClassId

namespace PluginId {
/** @brief Remote object access plugin (velk_remote). */
inline constexpr Uid RemotePlugin{"a49f30ac-ed40-467d-b4c0-74a7ed354192"};
} // namespace PluginId

} // namespace velk

#endif // VELK_PLUGINS_REMOTE_PLUGIN_H
