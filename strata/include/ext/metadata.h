#ifndef EXT_METADATA_H
#define EXT_METADATA_H

#include <array_view.h>
#include <interface/intf_metadata.h>
#include <interface/types.h>

#include <string_view>
#include <type_traits>
#include <utility>

namespace strata {

// -- Constexpr metadata collection from interfaces --

/** @brief Gets T::metadata if it exists, otherwise an empty array. */
template<class T, class = void>
struct type_metadata {
    static constexpr std::array<MemberDesc, 0> value{};
};

template<class T>
struct type_metadata<T, std::void_t<decltype(T::metadata)>> {
    static constexpr auto value = T::metadata;
};

/** @brief Constexpr concatenation of two std::arrays. */
template<class T, size_t N1, size_t N2>
constexpr std::array<T, N1 + N2> concat_arrays(std::array<T, N1> a, std::array<T, N2> b)
{
    std::array<T, N1 + N2> result{};
    for (size_t i = 0; i < N1; ++i) result[i] = a[i];
    for (size_t i = 0; i < N2; ++i) result[N1 + i] = b[i];
    return result;
}

/** @brief Collects metadata arrays from all interfaces into a single array. */
template<class...>
struct collected_metadata;

template<>
struct collected_metadata<> {
    static constexpr std::array<MemberDesc, 0> value{};
};

template<class First, class... Rest>
struct collected_metadata<First, Rest...> {
    static constexpr auto value = concat_arrays(
        type_metadata<First>::value,
        collected_metadata<Rest...>::value
    );
};

} // namespace strata

#endif // EXT_METADATA_H
