#include "raw_hive.h"

#include <cstring>

namespace velk {

RawHiveImpl::~RawHiveImpl()
{
    // Free all page allocations without calling destructors (type-erased).
    // Callers using RawHiveImpl<T> get automatic cleanup via its destructor.
    clear(nullptr, nullptr);
}

void RawHiveImpl::init(Uid elementUid, size_t elementSize, size_t elementAlign)
{
    element_uid_ = elementUid;
    slot_size_ = align_up(elementSize, elementAlign);
    if (slot_size_ < sizeof(size_t)) {
        slot_size_ = sizeof(size_t);
    }
    slot_align_ = elementAlign;
}

Uid RawHiveImpl::get_element_uid() const
{
    return element_uid_;
}

size_t RawHiveImpl::size() const
{
    return live_count_;
}

bool RawHiveImpl::empty() const
{
    return live_count_ == 0;
}

void* RawHiveImpl::slot_ptr(const SimpleHivePage& page, size_t index) const
{
    return static_cast<char*>(page.slots) + index * slot_size_;
}

void RawHiveImpl::alloc_page(size_t capacity)
{
    auto page = std::make_unique<SimpleHivePage>();
    page->capacity = capacity;
    page->slot_size = slot_size_;

    size_t num_words = bitmask_words(capacity);

    // Layout: [ uint64_t[bitmask] | pad | slots ]
    size_t bits_bytes = num_words * sizeof(uint64_t);
    size_t alloc_align = slot_align_ > alignof(uint64_t) ? slot_align_ : alignof(uint64_t);
    size_t slots_offset = align_up(bits_bytes, alloc_align);
    size_t slots_bytes = capacity * slot_size_;
    size_t total = slots_offset + slots_bytes;

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

void* RawHiveImpl::allocate()
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

    size_t word = slot_idx / 64;
    size_t bit = slot_idx % 64;
    target->active_bits[word] |= uint64_t(1) << bit;
    ++target->live_count;
    ++live_count_;

    return slot_ptr(*target, slot_idx);
}

void RawHiveImpl::deallocate(void* ptr)
{
    std::lock_guard<std::shared_mutex> lock(mutex_);
    auto ptr_addr = reinterpret_cast<uintptr_t>(ptr);
    for (auto& page_ptr : pages_) {
        auto& page = *page_ptr;
        auto base = reinterpret_cast<uintptr_t>(page.slots);
        auto end = base + page.capacity * slot_size_;
        if (ptr_addr >= base && ptr_addr < end) {
            size_t offset = static_cast<size_t>(ptr_addr - base);
            if (offset % slot_size_ != 0) {
                return;
            }
            size_t slot_idx = offset / slot_size_;

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

bool RawHiveImpl::contains(const void* ptr) const
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

void RawHiveImpl::for_each(void* context, RawVisitorFn visitor) const
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

                if (!visitor(context, slot_ptr(page, i))) {
                    return;
                }
            }
        }
    }
}

void RawHiveImpl::clear(void* context, DestroyFn destroy)
{
    std::lock_guard<std::shared_mutex> lock(mutex_);
    for (auto& page_ptr : pages_) {
        auto& page = *page_ptr;
        if (destroy) {
            size_t num_words = bitmask_words(page.capacity);
            for (size_t w = 0; w < num_words; ++w) {
                uint64_t bits = page.active_bits[w];
                while (bits) {
                    unsigned b = bitscan_forward64(bits);
                    bits &= bits - 1;
                    size_t i = w * 64 + b;
                    destroy(context, slot_ptr(page, i));
                }
            }
        }
        aligned_free_impl(page.allocation);
    }
    pages_.clear();
    current_page_ = nullptr;
    live_count_ = 0;
}

} // namespace velk
