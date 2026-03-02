#ifndef VELK_ANIMATOR_INTERPOLATOR_TRAITS_H
#define VELK_ANIMATOR_INTERPOLATOR_TRAITS_H

#include <velk/interface/intf_any.h>
#include <velk/interface/types.h>

namespace velk {

/**
 * @brief Default linear interpolation trait.
 *
 * Works for any type that supports `a + (b - a) * t`. Users may specialize
 * this for custom types (e.g. color, vector).
 */
template <class T>
struct interpolator_trait
{
    static T interpolate(const T& a, const T& b, float t)
    {
        return static_cast<T>(a + (b - a) * t);
    }
};

namespace detail {

/** @brief Typed interpolator callback: reads from/to IAny values, interpolates, writes to result IAny. */
template <class T>
ReturnValue typed_interpolator(const IAny& from, const IAny& to, float t, IAny& result)
{
    constexpr auto size = sizeof(T);
    constexpr auto uid = type_uid<T>();
    T a{}, b{};
    if (succeeded(from.get_data(&a, size, uid)) && succeeded(to.get_data(&b, size, uid))) {
        T r = interpolator_trait<T>::interpolate(a, b, t);
        return result.set_data(&r, size, uid);
    }
    return ReturnValue::Fail;
}

} // namespace detail
} // namespace velk

#endif // VELK_ANIMATOR_INTERPOLATOR_TRAITS_H
