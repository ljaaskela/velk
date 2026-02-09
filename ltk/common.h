#ifndef COMMON_H
#define COMMON_H

#include <memory>
#include <string_view>
#include <iostream>

#include <array>
#include <vector>

using std::array;
using std::vector;

using Uid = uint64_t;

using std::vector;

constexpr Uid make_hash(const std::string_view toHash)
{
    static_assert(sizeof(Uid) == 8);
    // FNV-1a 64 bit algorithm
    size_t result = 0xcbf29ce484222325; // FNV offset basis

    for (char c : toHash) {
        result ^= c;
        result *= 1099511628211; // FNV prime
    }

    return result;
}

template<std::size_t... Idxs>
constexpr auto substring_as_array(std::string_view str, std::index_sequence<Idxs...>)
{
    return std::array{str[Idxs]..., '\0'};
}

template<typename T>
constexpr auto type_name_array()
{
#if defined(__clang__)
    constexpr auto prefix = std::string_view{"[T = "};
    constexpr auto suffix = std::string_view{"]"};
    constexpr auto function = std::string_view{__PRETTY_FUNCTION__};
#elif defined(__GNUC__)
    constexpr auto prefix = std::string_view{"with T = "};
    constexpr auto suffix = std::string_view{"]"};
    constexpr auto function = std::string_view{__PRETTY_FUNCTION__};
#elif defined(_MSC_VER)
    constexpr auto prefix = std::string_view{"type_name_array<"};
    constexpr auto suffix = std::string_view{">(void)"};
    constexpr auto function = std::string_view{__FUNCSIG__};
#else
#error Unsupported compiler
#endif
    constexpr auto start = function.find(prefix) + prefix.size();
    constexpr auto end = function.rfind(suffix);
    static_assert(start < end);
    constexpr auto name = function.substr(start, (end - start));
    return substring_as_array(name, std::make_index_sequence<name.size()>{});
}

template<typename T>
struct type_name_holder
{
    static inline constexpr auto value = type_name_array<T>();
};

template<typename T>
constexpr std::string_view GetName()
{
    constexpr auto &value = type_name_holder<T>::value;
    return std::string_view{value.data(), value.size()};
}

template<class... T>
constexpr Uid TypeUid()
{
    return make_hash(GetName<T...>());
}

#endif // COMMON_H
