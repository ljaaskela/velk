#ifndef VELK_ANIMATOR_EASING_H
#define VELK_ANIMATOR_EASING_H

#include <cmath>

namespace velk::easing {

/** @brief Easing function signature: maps t in [0,1] to an interpolation value. */
using EasingFn = float (*)(float);

constexpr float linear(float t) { return t; }

constexpr float in_quad(float t) { return t * t; }
constexpr float out_quad(float t) { return t * (2.f - t); }
constexpr float in_out_quad(float t)
{
    return t < 0.5f ? 2.f * t * t : -1.f + (4.f - 2.f * t) * t;
}

constexpr float in_cubic(float t) { return t * t * t; }
constexpr float out_cubic(float t)
{
    float u = t - 1.f;
    return u * u * u + 1.f;
}
constexpr float in_out_cubic(float t)
{
    return t < 0.5f ? 4.f * t * t * t : (t - 1.f) * (2.f * t - 2.f) * (2.f * t - 2.f) + 1.f;
}

inline float in_sine(float t)
{
    return 1.f - std::cos(t * 3.14159265358979323846f * 0.5f);
}
inline float out_sine(float t)
{
    return std::sin(t * 3.14159265358979323846f * 0.5f);
}
inline float in_out_sine(float t)
{
    return 0.5f * (1.f - std::cos(3.14159265358979323846f * t));
}

inline float in_expo(float t)
{
    return t == 0.f ? 0.f : std::pow(2.f, 10.f * (t - 1.f));
}
inline float out_expo(float t)
{
    return t == 1.f ? 1.f : 1.f - std::pow(2.f, -10.f * t);
}
inline float in_out_expo(float t)
{
    if (t == 0.f) return 0.f;
    if (t == 1.f) return 1.f;
    return t < 0.5f ? 0.5f * std::pow(2.f, 20.f * t - 10.f)
                    : 1.f - 0.5f * std::pow(2.f, -20.f * t + 10.f);
}

inline float in_elastic(float t)
{
    if (t == 0.f || t == 1.f) return t;
    return -std::pow(2.f, 10.f * t - 10.f) *
           std::sin((t * 10.f - 10.75f) * (2.f * 3.14159265358979323846f / 3.f));
}
inline float out_elastic(float t)
{
    if (t == 0.f || t == 1.f) return t;
    return std::pow(2.f, -10.f * t) *
               std::sin((t * 10.f - 0.75f) * (2.f * 3.14159265358979323846f / 3.f)) +
           1.f;
}

inline float out_bounce(float t)
{
    if (t < 1.f / 2.75f) {
        return 7.5625f * t * t;
    } else if (t < 2.f / 2.75f) {
        t -= 1.5f / 2.75f;
        return 7.5625f * t * t + 0.75f;
    } else if (t < 2.5f / 2.75f) {
        t -= 2.25f / 2.75f;
        return 7.5625f * t * t + 0.9375f;
    } else {
        t -= 2.625f / 2.75f;
        return 7.5625f * t * t + 0.984375f;
    }
}
inline float in_bounce(float t) { return 1.f - out_bounce(1.f - t); }

} // namespace velk::easing

#endif // VELK_ANIMATOR_EASING_H
