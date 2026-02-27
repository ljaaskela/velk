#ifndef VELK_HIVE_H
#define VELK_HIVE_H

#include "page_allocator.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <shared_mutex>
#include <vector>

namespace velk {

/**
 * @brief Page for the simple Hive<T> allocator.
 *
 * Stores a contiguous array of T slots with an active bitmask and
 * intrusive freelist. No reference counting, zombie, or orphan
 * management (those belong to ObjectHive).
 */
struct SimpleHivePage
{
    void* allocation{nullptr};
    uint64_t* active_bits{nullptr};
    void* slots{nullptr};
    size_t capacity{0};
    size_t free_head{PAGE_SENTINEL};
    size_t live_count{0};
    size_t slot_size{0};
};

/**
 * @brief Simple page-based pool allocator for non-ref-counted objects.
 *
 * Provides O(1) allocation, O(1) deallocation, and cache-friendly
 * contiguous storage. Objects are constructed in-place via emplace()
 * and destroyed + reclaimed via deallocate(). The destructor destroys
 * all live objects and frees all pages.
 *
 * @tparam T The object type to store.
 */
template <class T>
class Hive
{
public:
    Hive()
    {
        slot_size_ = align_up(sizeof(T), alignof(T));
        if (slot_size_ < sizeof(size_t)) {
            slot_size_ = sizeof(size_t);
        }
    }

    ~Hive()
    {
        for (auto& page_ptr : pages_) {
            auto& page = *page_ptr;
            size_t num_words = bitmask_words(page.capacity);
            for (size_t w = 0; w < num_words; ++w) {
                uint64_t bits = page.active_bits[w];
                while (bits) {
                    unsigned b = bitscan_forward64(bits);
                    bits &= bits - 1;
                    size_t i = w * 64 + b;
                    auto* obj = static_cast<T*>(slot_ptr(page, i));
                    obj->~T();
                }
            }
            aligned_free_impl(page.allocation);
        }
    }

    Hive(const Hive&) = delete;
    Hive& operator=(const Hive&) = delete;

    /** @brief Constructs a T in-place with the given arguments. Thread-safe (exclusive lock). */
    template <class... Args>
    T* emplace(Args&&... args)
    {
        std::lock_guard<std::shared_mutex> lock(mutex_);
        SimpleHivePage* target = nullptr;
        if (current_page_ && current_page_->free_head != PAGE_SENTINEL) {
            target = current_page_;
        } else {
            for (auto& page_ptr : pages_) {
                if (page_ptr->free_head != PAGE_SENTINEL) {
                    target = page_ptr.get();
                    break;
                }
            }
        }

        if (!target) {
            alloc_page(next_page_capacity(pages_.size()));
            target = pages_.back().get();
        }

        current_page_ = target;

        size_t slot_idx = pop_free_slot(target->slots, slot_size_, target->free_head);

        // Set active bit.
        size_t word = slot_idx / 64;
        size_t bit = slot_idx % 64;
        target->active_bits[word] |= uint64_t(1) << bit;
        ++target->live_count;
        ++live_count_;

        void* slot = slot_ptr(*target, slot_idx);
        return new (slot) T(static_cast<Args&&>(args)...);
    }

    /** @brief Destroys the object and reclaims its slot. Thread-safe (exclusive lock). */
    void deallocate(T* ptr)
    {
        std::lock_guard<std::shared_mutex> lock(mutex_);
        for (auto& page_ptr : pages_) {
            auto& page = *page_ptr;
            auto ptr_addr = reinterpret_cast<uintptr_t>(ptr);
            auto base = reinterpret_cast<uintptr_t>(page.slots);
            auto end = base + page.capacity * slot_size_;
            if (ptr_addr >= base && ptr_addr < end) {
                size_t offset = static_cast<size_t>(ptr_addr - base);
                if (offset % slot_size_ != 0) {
                    return;
                }
                size_t slot_idx = offset / slot_size_;

                ptr->~T();

                // Clear active bit.
                size_t word = slot_idx / 64;
                size_t bit = slot_idx % 64;
                page.active_bits[word] &= ~(uint64_t(1) << bit);

                push_free_slot(page.slots, slot_idx, slot_size_, page.free_head);
                --page.live_count;
                --live_count_;
                return;
            }
        }
    }

    /** @brief Returns true if the pointer belongs to this hive. Thread-safe (shared lock). */
    bool contains(const T* ptr) const
    {
        std::shared_lock lock(mutex_);
        auto ptr_addr = reinterpret_cast<uintptr_t>(ptr);
        for (auto& page_ptr : pages_) {
            auto& page = *page_ptr;
            auto base = reinterpret_cast<uintptr_t>(page.slots);
            auto end = base + page.capacity * slot_size_;
            if (ptr_addr >= base && ptr_addr < end) {
                size_t offset = static_cast<size_t>(ptr_addr - base);
                if (offset % slot_size_ != 0) {
                    return false;
                }
                size_t slot_idx = offset / slot_size_;
                size_t word = slot_idx / 64;
                size_t bit = slot_idx % 64;
                return (page.active_bits[word] & (uint64_t(1) << bit)) != 0;
            }
        }
        return false;
    }

    /** @brief Returns the number of live objects. */
    size_t size() const { return live_count_; }

    /** @brief Returns true if the hive contains no live objects. */
    bool empty() const { return live_count_ == 0; }

    /** @brief Iterates all live objects. fn(T&) returns void or bool (false to stop). Thread-safe (shared lock). */
    template <class Fn>
    void for_each(Fn&& fn) const
    {
        std::shared_lock lock(mutex_);
        for (auto& page_ptr : pages_) {
            auto& page = *page_ptr;
            size_t num_words = bitmask_words(page.capacity);
            for (size_t w = 0; w < num_words; ++w) {
                uint64_t bits = page.active_bits[w];
                while (bits) {
                    unsigned b = bitscan_forward64(bits);
                    bits &= bits - 1;
                    size_t i = w * 64 + b;

                    // Prefetch next active slot.
                    uint64_t remaining = bits;
                    if (remaining) {
                        unsigned nb = bitscan_forward64(remaining);
                        prefetch_line(slot_ptr(page, w * 64 + nb));
                    } else if (w + 1 < num_words) {
                        for (size_t nw = w + 1; nw < num_words; ++nw) {
                            uint64_t next_bits = page.active_bits[nw];
                            if (next_bits) {
                                prefetch_line(slot_ptr(page, nw * 64 + bitscan_forward64(next_bits)));
                                break;
                            }
                        }
                    }

                    auto* obj = static_cast<T*>(slot_ptr(page, i));
                    if constexpr (std::is_same_v<decltype(fn(*obj)), bool>) {
                        if (!fn(*obj)) {
                            return;
                        }
                    } else {
                        fn(*obj);
                    }
                }
            }
        }
    }

private:
    void* slot_ptr(const SimpleHivePage& page, size_t index) const
    {
        return static_cast<char*>(page.slots) + index * slot_size_;
    }

    void alloc_page(size_t capacity)
    {
        auto page = std::make_unique<SimpleHivePage>();
        page->capacity = capacity;
        page->slot_size = slot_size_;

        size_t num_words = bitmask_words(capacity);

        // Layout: [ uint64_t[bitmask] | pad | T[capacity] ]
        size_t bits_bytes = num_words * sizeof(uint64_t);
        size_t slots_offset = align_up(bits_bytes, alignof(T) > alignof(uint64_t) ? alignof(T) : alignof(uint64_t));
        size_t slots_bytes = capacity * slot_size_;
        size_t total = slots_offset + slots_bytes;

        size_t alloc_align = alignof(T) > alignof(uint64_t) ? alignof(T) : alignof(uint64_t);
        auto* mem = static_cast<char*>(aligned_alloc_impl(alloc_align, total));
        page->allocation = mem;
        page->active_bits = reinterpret_cast<uint64_t*>(mem);
        page->slots = mem + slots_offset;

        std::memset(page->active_bits, 0, bits_bytes);

        build_freelist(page->slots, capacity, slot_size_, page->free_head);
        page->live_count = 0;

        current_page_ = page.get();
        pages_.push_back(std::move(page));
    }

    mutable std::shared_mutex mutex_;
    size_t slot_size_{0};
    size_t live_count_{0};
    SimpleHivePage* current_page_{nullptr};
    std::vector<std::unique_ptr<SimpleHivePage>> pages_;
};

} // namespace velk

#endif // VELK_HIVE_H
