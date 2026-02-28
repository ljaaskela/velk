#ifndef VELK_COMMON_H
#define VELK_COMMON_H

#include <velk/uid.h>

#include <array>

// MSVC does not apply Empty Base Optimization to classes with multiple inheritance
// unless __declspec(empty_bases) is specified. Apply to empty mixin bases so that
// user code combining them (e.g. struct Foo : NoCopy, NoMove {}) does not pay 8 bytes.
#ifdef _MSC_VER
#define VELK_EMPTY_BASES __declspec(empty_bases)
#else
#define VELK_EMPTY_BASES
#endif

namespace velk {

/** @brief Extracts a substring into a null-terminated std::array at compile time. */
template <std::size_t... Idxs>
constexpr auto substring_as_array(string_view str, std::index_sequence<Idxs...>)
{
    return std::array{str[Idxs]..., '\0'};
}

/** @brief Returns the compiler-deduced name of T as a null-terminated std::array. */
template <typename T>
constexpr auto type_name_array()
{
#if defined(__clang__)
    constexpr auto prefix = string_view{"[T = "};
    constexpr auto suffix = string_view{"]"};
    constexpr auto function = string_view{__PRETTY_FUNCTION__};
#elif defined(__GNUC__)
    constexpr auto prefix = string_view{"with T = "};
    constexpr auto suffix = string_view{"]"};
    constexpr auto function = string_view{__PRETTY_FUNCTION__};
#elif defined(_MSC_VER)
    constexpr auto prefix = string_view{"type_name_array<"};
    constexpr auto suffix = string_view{">(void)"};
    constexpr auto function = string_view{__FUNCSIG__};
#else
#error Unsupported compiler
#endif
    constexpr auto start = function.find(prefix) + prefix.size();
    constexpr auto end = function.rfind(suffix);
    static_assert(start < end);
    constexpr auto name = function.substr(start, (end - start));
    return substring_as_array(name, std::make_index_sequence<name.size()>{});
}

/** @brief Holds the compile-time name array for type T as a static constexpr member. */
template <typename T>
struct type_name_holder
{
    static inline constexpr auto value = type_name_array<T>();
};

/**
 * @brief Returns the compile-time name of type T as a string_view.
 * @tparam T The type whose name to retrieve.
 */
template <typename T>
constexpr string_view get_name()
{
    constexpr auto& value = type_name_holder<T>::value;
    return string_view{value.data(), value.size()};
}

/**
 * @brief Returns a unique compile-time identifier for type T, derived from its name.
 * @tparam T The type to identify.
 */
template <class... T>
constexpr Uid type_uid()
{
    return make_hash(get_name<T...>());
}

/** @brief Mixin that deletes copy constructor and copy assignment operator. */
struct VELK_EMPTY_BASES NoCopy
{
    NoCopy() = default;
    ~NoCopy() = default;
    NoCopy(const NoCopy&) = delete;
    NoCopy& operator=(const NoCopy&) = delete;
};

/** @brief Mixin that deletes move constructor and move assignment operator. */
struct VELK_EMPTY_BASES NoMove
{
    NoMove() = default;
    ~NoMove() = default;
    NoMove(NoMove&&) = delete;
    NoMove& operator=(NoMove&&) = delete;
};

/** @brief Mixin that prevents both copying and moving. */
struct VELK_EMPTY_BASES NoCopyMove
{
    NoCopyMove() = default;
    ~NoCopyMove() = default;
    NoCopyMove(const NoCopyMove&) = delete;
    NoCopyMove& operator=(const NoCopyMove&) = delete;
    NoCopyMove(NoCopyMove&&) = delete;
    NoCopyMove& operator=(NoCopyMove&&) = delete;
};

} // namespace velk

#endif // VELK_COMMON_H
