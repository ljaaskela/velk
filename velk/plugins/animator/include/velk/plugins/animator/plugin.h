#ifndef VELK_PLUGINS_ANIMATOR_PLUGIN_H
#define VELK_PLUGINS_ANIMATOR_PLUGIN_H

#include <velk/common.h>

namespace velk {

namespace ClassId {
/** @brief Animation (keyframe-based, including simple tweens). */
inline constexpr Uid Animation{"786c599c-2d55-4fb4-935e-a73d190af351"};
/** @brief Animator that manages and ticks a set of animations. */
inline constexpr Uid Animator{"3995b907-755d-4c3f-8595-323b25b3fb03"};
} // namespace ClassId

namespace PluginId {
/** @brief Property animation plugin (velk_animator). */
inline constexpr Uid AnimatorPlugin{"738617b8-dba8-4a08-a0bc-51e8dc4d5faf"};
} // namespace PluginId

} // namespace velk

#endif // VELK_PLUGINS_ANIMATOR_PLUGIN_H
