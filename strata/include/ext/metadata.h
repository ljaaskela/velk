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
struct TypeMetadata {
    static constexpr std::array<MemberDesc, 0> value{};
};

/** @brief Specialization that extracts T::metadata when it exists. */
template<class T>
struct TypeMetadata<T, std::void_t<decltype(T::metadata)>> {
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
struct CollectedMetadata;

/** @brief Base case: no interfaces yield an empty metadata array. */
template<>
struct CollectedMetadata<> {
    static constexpr std::array<MemberDesc, 0> value{};
};

/** @brief Recursive case: concatenates First::metadata with the rest. */
template<class First, class... Rest>
struct CollectedMetadata<First, Rest...> {
    static constexpr auto value = concat_arrays(
        TypeMetadata<First>::value,
        CollectedMetadata<Rest...>::value
    );
};

// -- State struct extraction from interfaces --

/** @brief Extracts T::State if it exists, otherwise provides an empty struct. */
template<class T, class = void>
struct InterfaceState {
    struct type {};
};

/** @brief Specialization that extracts T::State when it exists. */
template<class T>
struct InterfaceState<T, std::void_t<typename T::State>> {
    using type = typename T::State;
};

} // namespace strata

#endif // EXT_METADATA_H
