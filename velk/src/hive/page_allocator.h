#ifndef VELK_PAGE_ALLOCATOR_H
#define VELK_PAGE_ALLOCATOR_H

#include <velk/interface/hive/intf_hive.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

#ifdef _WIN32
#include <intrin.h>
#include <malloc.h>
#else
#include <cstdlib>
#endif

namespace velk {

static constexpr size_t PAGE_SENTINEL = ~size_t(0);

inline void* aligned_alloc_impl(size_t alignment, size_t size)
{
#ifdef _WIN32
    return _aligned_malloc(size, alignment);
#else
    return std::aligned_alloc(alignment, size);
#endif
}

inline void aligned_free_impl(void* ptr)
{
#ifdef _WIN32
    _aligned_free(ptr);
#else
    std::free(ptr);
#endif
}

inline size_t align_up(size_t size, size_t alignment)
{
    return (size + alignment - 1) & ~(alignment - 1);
}

inline void prefetch_line(const void* addr)
{
#ifdef _WIN32
    _mm_prefetch(static_cast<const char*>(addr), _MM_HINT_T0);
#else
    __builtin_prefetch(addr, 0, 1);
#endif
}

/** @brief Returns the index of the lowest set bit, or 64 if none. */
inline unsigned bitscan_forward64(uint64_t mask)
{
#ifdef _WIN32
    unsigned long idx;
    if (_BitScanForward64(&idx, mask)) {
        return static_cast<unsigned>(idx);
    }
    return 64;
#else
    if (mask == 0) {
        return 64;
    }
    return static_cast<unsigned>(__builtin_ctzll(mask));
#endif
}

/** @brief Number of uint64_t words needed for a bitmask covering @p capacity slots. */
inline size_t bitmask_words(size_t capacity)
{
    return (capacity + 63) / 64;
}

/** @brief Sets the active bit for slot @p bit in bitmask word @p word. */
inline void set_slot_active(uint64_t* active_bits, size_t word, size_t bit)
{
    active_bits[word] |= uint64_t(1) << (bit & 63);
}

/** @brief Clears the active bit for slot @p bit in bitmask word @p word. */
inline void clear_slot_active(uint64_t* active_bits, size_t word, size_t bit)
{
    active_bits[word] &= ~(uint64_t(1) << (bit & 63));
}

/** @brief Tests whether slot @p bit in bitmask word @p word is active. */
inline bool is_slot_active(const uint64_t* active_bits, size_t word, size_t bit)
{
    return (active_bits[word] & (uint64_t(1) << (bit & 63))) != 0;
}

/** @brief Returns the page capacity for a given page index. */
inline size_t next_page_capacity(const HivePageCapacity& capacity, size_t page_count)
{
    switch (page_count) {
    case 0:
        return capacity.page_1;
    case 1:
        return capacity.page_2;
    case 2:
        return capacity.page_3;
    default:
        return capacity.page_n;
    }
}

inline HivePageCapacity check_capacity(const HivePageCapacity& capacity)
{
    auto max = [](const size_t& lhs, const size_t& rhs) noexcept { return lhs < rhs ? rhs : lhs; };

    auto rv = capacity;
    rv.page_1 = max(rv.page_1, size_t(1));
    rv.page_2 = max(rv.page_2, rv.page_1);
    rv.page_3 = max(rv.page_3, rv.page_2);
    rv.page_n = max(rv.page_n, rv.page_3);
    return rv;
}

/** @brief Builds an intrusive freelist through slot memory. */
inline void build_freelist(void* slots, size_t capacity, size_t slot_size, size_t& free_head)
{
    for (size_t i = 0; i < capacity - 1; ++i) {
        size_t next = i + 1;
        std::memcpy(static_cast<char*>(slots) + i * slot_size, &next, sizeof(size_t));
    }
    size_t sentinel = PAGE_SENTINEL;
    std::memcpy(static_cast<char*>(slots) + (capacity - 1) * slot_size, &sentinel, sizeof(size_t));
    free_head = 0;
}

/** @brief Pushes a slot onto a page's freelist. */
inline void push_free_slot(void* slots, size_t index, size_t slot_size, size_t& free_head)
{
    std::memcpy(static_cast<char*>(slots) + index * slot_size, &free_head, sizeof(size_t));
    free_head = index;
}

/** @brief Pops a slot from a page's freelist. Returns the slot index. */
inline size_t pop_free_slot(void* slots, size_t slot_size, size_t& free_head)
{
    size_t index = free_head;
    size_t next;
    std::memcpy(&next, static_cast<char*>(slots) + index * slot_size, sizeof(size_t));
    free_head = next;
    return index;
}

} // namespace velk

#endif // VELK_PAGE_ALLOCATOR_H
