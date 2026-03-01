#ifndef VELK_ANIMATOR_TRANSITION_H
#define VELK_ANIMATOR_TRANSITION_H

#include <velk/interface/types.h>
#include <velk/plugins/animator/easing.h>

namespace velk {

/** @brief Configuration for an implicit property transition. */
struct TransitionConfig
{
    Duration duration;
    easing::EasingFn easing = easing::linear;
};

} // namespace velk

#endif // VELK_ANIMATOR_TRANSITION_H
