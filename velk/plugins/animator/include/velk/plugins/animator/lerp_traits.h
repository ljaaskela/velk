#ifndef VELK_ANIMATOR_LERP_TRAITS_H
#define VELK_ANIMATOR_LERP_TRAITS_H

namespace velk {

/**
 * @brief Default linear interpolation trait.
 *
 * Works for any type that supports `a + (b - a) * t`. Users may specialize
 * this for custom types (e.g. color, vector).
 */
template <class T>
struct lerp_trait
{
    static T lerp(const T& a, const T& b, float t)
    {
        return static_cast<T>(a + (b - a) * t);
    }
};

} // namespace velk

#endif // VELK_ANIMATOR_LERP_TRAITS_H
