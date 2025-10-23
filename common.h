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

//template <typename Str>
constexpr Uid MakeHash(const std::string_view toHash)
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

template<class... T>
constexpr std::string_view GetName()
{
#ifdef _MSC_VER
    char const *p = __FUNCSIG__;
#else
    char const *p = __PRETTY_FUNCTION__;
#endif
    while (*p++ != '=');
    for (; *p == ' '; ++p);
    char const* p2 = p;
    int count = 1;
    for (;;++p2)
    {
        switch (*p2)
        {
        case '[':
            ++count;
            break;
        case ']':
            --count;
            if (!count)
                return {p, std::size_t(p2 - p)};
        }
    }
    return {};
}

template<class... T>
constexpr Uid TypeUid()
{
    return MakeHash(GetName<T...>());
}

#define MakeInterfaceUid(type) MakeHash(#type)
#define MakeClassUid(className) MakeHash(#className)

#endif // COMMON_H
