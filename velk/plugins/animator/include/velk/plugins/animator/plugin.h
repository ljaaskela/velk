#ifndef VELK_PLUGINS_ANIMATOR_PLUGIN_H
#define VELK_PLUGINS_ANIMATOR_PLUGIN_H

#include <velk/common.h>

namespace velk {

namespace ClassId {
/** @brief Animation (keyframe-based, including simple tweens). */
inline constexpr Uid Animation{"786c599c-2d55-4fb4-935e-a73d190af351"};
/** @brief Animator that manages and ticks a set of animations. */
inline constexpr Uid Animator{"3995b907-755d-4c3f-8595-323b25b3fb03"};
/** @brief First-class implicit property transition. */
inline constexpr Uid Transition{"d4f8e2a1-6b3c-4d97-a5e0-9c7f1b2d3e4a"};
} // namespace ClassId

namespace PluginId {
/** @brief Property animation plugin (velk_animator). */
inline constexpr Uid AnimatorPlugin{"738617b8-dba8-4a08-a0bc-51e8dc4d5faf"};
} // namespace PluginId

} // namespace velk

#endif // VELK_PLUGINS_ANIMATOR_PLUGIN_H
