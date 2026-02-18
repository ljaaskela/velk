#ifndef UID_H
#define UID_H

#include <cstdint>
#include <ostream>
#include <string_view.h>

namespace velk {

/** @brief Returns true if @p c is a valid hexadecimal digit. */
constexpr bool is_hex_digit(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

/** @brief Returns true if @p str is a valid UUID string (xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx). */
constexpr bool is_valid_uid_format(string_view str)
{
    if (str.size() != 36) return false;
    for (size_t i = 0; i < 36; ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (str[i] != '-') return false;
        } else {
            if (!is_hex_digit(str[i])) return false;
        }
    }
    return true;
}

/** @brief Called at runtime for malformed UID strings. Not constexpr, so bad UIDs in
 *  constexpr context produce a compile error. */
inline void uid_format_error() {
}

/** @brief 128-bit unique identifier used for type and interface identification. */
struct Uid {
    uint64_t hi = 0;
    uint64_t lo = 0;

    constexpr Uid() = default;
    constexpr Uid(uint64_t h, uint64_t l) : hi(h), lo(l) {}

    /** @brief Constructs a Uid from a string literal. Validates length at compile time. */
    template<size_t N>
    constexpr Uid(const char (&str)[N]) : Uid(string_view(str, N - 1))
    {
        static_assert(N == 37, "Uid string must be 36 characters (xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx)");
    }

    /** @brief Constructs a Uid from a UUID string (e.g. "cc262192-d151-941f-d542-d4c622b50b09").
     *  In constexpr context, a malformed string produces a compile error. */
    constexpr Uid(string_view str) : hi(0), lo(0)
    {
        if (!is_valid_uid_format(str)) {
            // Non-constexpr call makes the compiler reject bad UIDs in constexpr context.
            // At runtime, produces a zero Uid.
            uid_format_error();
            return;
        }
        for (auto c : str) {
            if (c == '-') continue;
            uint64_t nibble = (c >= '0' && c <= '9') ? uint64_t(c - '0') :
                              (c >= 'a' && c <= 'f') ? uint64_t(c - 'a' + 10) :
                              (c >= 'A' && c <= 'F') ? uint64_t(c - 'A' + 10) : 0;
            hi = (hi << 4) | (lo >> 60);
            lo = (lo << 4) | nibble;
        }
    }

    constexpr bool operator==(const Uid& o) const { return hi == o.hi && lo == o.lo; }
    constexpr bool operator!=(const Uid& o) const { return !(*this == o); }
    constexpr bool operator<(const Uid& o) const { return hi < o.hi || (hi == o.hi && lo < o.lo); }

    friend std::ostream& operator<<(std::ostream& os, const Uid& u)
    {
        constexpr char hex[] = "0123456789abcdef";
        auto put = [&](uint64_t v, int nibbles) {
            for (int i = (nibbles - 1) * 4; i >= 0; i -= 4)
                os.put(hex[(v >> i) & 0xF]);
        };
        // GUID format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
        put(u.hi >> 32, 8); os.put('-');
        put(u.hi >> 16, 4); os.put('-');
        put(u.hi,       4); os.put('-');
        put(u.lo >> 48, 4); os.put('-');
        put(u.lo, 12);
        return os;
    }
};

static_assert(sizeof(Uid) == 16);

/**
 * @brief Constexpr helper: returns the high 64 bits of a 64x64 multiply.
 */
constexpr uint64_t mulhi64(uint64_t a, uint64_t b)
{
    uint64_t a_lo = a & 0xFFFFFFFF;
    uint64_t a_hi = a >> 32;
    uint64_t b_lo = b & 0xFFFFFFFF;
    uint64_t b_hi = b >> 32;

    uint64_t lo_lo = a_lo * b_lo;
    uint64_t lo_hi = a_lo * b_hi;
    uint64_t hi_lo = a_hi * b_lo;
    uint64_t hi_hi = a_hi * b_hi;

    uint64_t cross = (lo_lo >> 32) + (lo_hi & 0xFFFFFFFF) + (hi_lo & 0xFFFFFFFF);
    return hi_hi + (lo_hi >> 32) + (hi_lo >> 32) + (cross >> 32);
}

/**
 * @brief Multiplies a 128-bit value by the FNV-128 prime (2^88 + 315).
 *
 * Exploits the prime's structure: x * prime = (x << 88) + x * 315.
 */
constexpr Uid fnv128_multiply(Uid x)
{
    // Compute x * 315 as a 128-bit result
    constexpr uint64_t small = 315;
    uint64_t mul_lo = x.lo * small;
    uint64_t mul_hi = mulhi64(x.lo, small) + x.hi * small;

    // Compute x << 88 = (x << 64) << 24. The low 64 bits are always 0.
    // High 64 bits = x.lo << 24 (since x.hi bits shift out entirely for a 128-bit value)
    uint64_t shift_hi = x.lo << 24;

    // Add: (shift_hi, 0) + (mul_hi, mul_lo)
    uint64_t res_lo = mul_lo;
    uint64_t res_hi = mul_hi + shift_hi;

    return {res_hi, res_lo};
}

/**
 * @brief Computes a compile-time FNV-1a 128-bit hash of a string.
 * @param toHash The string to hash.
 * @return The computed 128-bit Uid.
 */
constexpr Uid make_hash(const string_view toHash)
{
    // FNV-1a 128-bit: offset basis and prime from the FNV spec
    Uid result{0x6c62272e07bb0142, 0x62b821756295c58d};

    for (char c : toHash) {
        result.lo ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
        result = fnv128_multiply(result);
    }

    return result;
}

} // namespace velk

/** @brief Expands a UUID string literal into two uint64_t template arguments (hi, lo). */
#define VELK_UID(str) ::velk::Uid(str).hi, ::velk::Uid(str).lo

/** @brief Declares a static constexpr class UID from a UUID string literal. */
#define VELK_CLASS_UID(str) static constexpr ::velk::Uid class_uid{str}

#endif // UID_H
