#include "object_hive.h"

#include "page_allocator.h"

#include <velk/api/velk.h>
#include <velk/interface/intf_metadata.h>

#include <new>

namespace velk {

/**
 * @brief Extended control block for hive-managed objects.
 *
 * Stores only the ECB and a back-pointer to the page. The slot index
 * is recovered from the object address; factory and slot_size live on
 * the page. These are embedded in the page allocation (not individually
 * heap-allocated).
 */
struct HiveControlBlock
{
    external_control_block ecb;
    HivePage* page;
};

/**
 * @brief Shared destroy logic for hive-managed objects.
 *
 * Called from unref() when the last strong reference drops. Recovers the
 * HiveControlBlock context and destroys the object in place, returning
 * the slot to the page freelist (normal mode) or freeing the orphaned
 * page when the last object on it is destroyed (orphan mode).
 */
static void hive_destroy_impl(HiveControlBlock* hcb, bool orphan)
{
    HivePage* page = hcb->page;
    size_t slot_sz = page->slot_size;
    const IObjectFactory* factory = page->factory;

    // Recover slot index from the object address.
    auto obj_addr = reinterpret_cast<uintptr_t>(hcb->ecb.get_ptr());
    auto base_addr = reinterpret_cast<uintptr_t>(page->slots);
    size_t slot_index = static_cast<size_t>(obj_addr - base_addr) / slot_sz;

    auto* slot = static_cast<char*>(page->slots) + slot_index * slot_sz;

    // Prevent the block from being freed during the destructor chain.
    // ~RefCountedDispatch calls release_ref_counted -> release_control_block
    // -> release_weak(). Our bump keeps the block alive through the destructor.
    hcb->ecb.add_weak();

    factory->destroy_in_place(slot);

    // The destructor decremented weak by 1. Our bump kept the block alive.
    // Now release our extra weak. For embedded blocks, dealloc_control_block
    // will see the embedded tag and skip deletion. If external+embedded,
    // it calls ecb->destroy as a weak dealloc notification.
    bool last_weak = hcb->ecb.release_weak();

    if (!orphan) {
        // Lock the owning hive's mutex to protect page state (freelist, bitmask, counts).
        std::lock_guard<std::shared_mutex> lock(*page->hive_mutex);

        // Normal mode: block stays embedded in the page. If outstanding
        // weak_ptrs exist, set the destroy callback to hive_weak_release
        // so the page can track when they all drop.
        if (!last_weak) {
            // Outstanding weak_ptrs. Set destroy to the weak release callback
            // so dealloc_control_block (called when last weak_ptr drops) will
            // notify the page. The external+embedded tags are already set.
            // No weak_hcb_count tracking needed in normal mode because the
            // page is still owned by the Hive.
            hcb->ecb.destroy = nullptr;
        }
        // else: no outstanding weak_ptrs, block is fully dead. It sits inert
        // in the page allocation, ready for reuse.

        // Clear active bit and transition slot to Free.
        size_t word = slot_index / 64;
        size_t bit = slot_index % 64;
        page->active_bits[word] &= ~(uint64_t(1) << bit);

        page->state[slot_index] = SlotState::Free;
        push_free_slot(page->slots, slot_index, slot_sz, page->free_head);
    } else {
        // Orphan mode: page was detached from the Hive.
        if (!last_weak) {
            // Outstanding weak_ptrs. We need to track this so the page can
            // be freed when all weak_ptrs drop.
            hcb->ecb.destroy = [](external_control_block* ecb) {
                auto* h = reinterpret_cast<HiveControlBlock*>(ecb);
                HivePage* p = h->page;
                if (p->weak_hcb_count.fetch_sub(1, std::memory_order_acq_rel) == 1 && p->live_count == 0) {
                    aligned_free_impl(p->allocation);
                    delete p;
                }
            };
            page->weak_hcb_count.fetch_add(1, std::memory_order_relaxed);
        }

        page->state[slot_index] = SlotState::Free;
    }
    --page->live_count;

    if (orphan && page->live_count == 0 && page->weak_hcb_count.load(std::memory_order_acquire) == 0) {
        aligned_free_impl(page->allocation);
        delete page;
    }
}

static void hive_destroy(external_control_block* ecb)
{
    hive_destroy_impl(reinterpret_cast<HiveControlBlock*>(ecb), false);
}

static void hive_destroy_orphan(external_control_block* ecb)
{
    hive_destroy_impl(reinterpret_cast<HiveControlBlock*>(ecb), true);
}

// --- Hive implementation ---

ObjectHive::~ObjectHive()
{
    for (auto& page_ptr : pages_) {
        auto& page = *page_ptr;
        size_t num_words = bitmask_words(page.capacity);

        // First pass: release the hive's strong ref on all Active objects.
        // Use the bitmask for fast scanning.
        for (size_t w = 0; w < num_words; ++w) {
            uint64_t bits = page.active_bits[w];
            while (bits) {
                unsigned bit = bitscan_forward64(bits);
                size_t i = w * 64 + bit;
                bits &= bits - 1; // clear lowest set bit

                // Clear active bit
                page.active_bits[w] &= ~(uint64_t(1) << bit);
                page.state[i] = SlotState::Zombie;
                auto* obj = static_cast<IObject*>(slot_ptr(page, i));
                obj->unref(); // Release hive's strong ref
            }
        }

        // Second pass: check if any zombies or outstanding weak HCBs remain.
        bool has_zombies = false;
        for (size_t i = 0; i < page.capacity; ++i) {
            if (page.state[i] == SlotState::Zombie) {
                has_zombies = true;
                break;
            }
        }

        bool has_weak_hcbs = page.weak_hcb_count.load(std::memory_order_acquire) > 0;

        if (has_zombies || has_weak_hcbs) {
            // Transfer page ownership to an orphan. Null out the mutex pointer
            // since the ObjectHive (and its mutex) are being destroyed.
            page.orphaned = true;
            page.hive_mutex = nullptr;
            for (size_t i = 0; i < page.capacity; ++i) {
                if (page.state[i] == SlotState::Zombie) {
                    page.hcbs[i].ecb.destroy = hive_destroy_orphan;
                }
            }
            // Release unique_ptr ownership.
            page_ptr.release();
        } else {
            free_page(page);
        }
    }
}

void ObjectHive::init(Uid classUid)
{
    element_class_uid_ = classUid;
    factory_ = instance().type_registry().find_factory(classUid);
    if (factory_) {
        slot_size_ = align_up(factory_->get_instance_size(), factory_->get_instance_alignment());
        slot_alignment_ = factory_->get_instance_alignment();
    }
}

Uid ObjectHive::get_element_uid() const
{
    return element_class_uid_;
}

size_t ObjectHive::size() const
{
    return live_count_;
}

bool ObjectHive::empty() const
{
    return live_count_ == 0;
}

void* ObjectHive::slot_ptr(HivePage& page, size_t index) const
{
    return static_cast<char*>(page.slots) + index * slot_size_;
}

void* ObjectHive::slot_ptr(const HivePage& page, size_t index) const
{
    return static_cast<char*>(page.slots) + index * slot_size_;
}

void ObjectHive::alloc_page(size_t capacity)
{
    auto page = std::make_unique<HivePage>();
    page->capacity = capacity;
    page->slot_size = slot_size_;
    page->factory = factory_;

    size_t num_words = bitmask_words(capacity);

    // Compute layout:
    // [ SlotState[capacity] | pad | uint64_t[bitmask] | pad | HiveControlBlock[capacity] | pad | slots ]
    size_t state_bytes = capacity * sizeof(SlotState);
    size_t bits_offset = align_up(state_bytes, alignof(uint64_t));
    size_t bits_bytes = num_words * sizeof(uint64_t);
    size_t hcbs_offset = align_up(bits_offset + bits_bytes, alignof(HiveControlBlock));
    size_t hcbs_bytes = capacity * sizeof(HiveControlBlock);
    size_t slots_offset = align_up(hcbs_offset + hcbs_bytes, slot_alignment_);
    size_t slots_bytes = capacity * slot_size_;
    size_t total = slots_offset + slots_bytes;

    auto* mem = static_cast<char*>(aligned_alloc_impl(slot_alignment_, total));
    page->allocation = mem;
    page->state = reinterpret_cast<SlotState*>(mem);
    page->active_bits = reinterpret_cast<uint64_t*>(mem + bits_offset);
    page->hcbs = reinterpret_cast<HiveControlBlock*>(mem + hcbs_offset);
    page->slots = mem + slots_offset;

    for (size_t i = 0; i < capacity; ++i) {
        page->state[i] = SlotState::Free;
    }
    std::memset(page->active_bits, 0, bits_bytes);

    // Build intrusive freelist through slot memory.
    build_freelist(page->slots, capacity, slot_size_, page->free_head);
    page->live_count = 0;

    page->hive_mutex = &mutex_;
    current_page_ = page.get();
    pages_.push_back(std::move(page));
}

void ObjectHive::free_page(HivePage& page)
{
    aligned_free_impl(page.allocation);
    page.allocation = nullptr;
    page.state = nullptr;
    page.active_bits = nullptr;
    page.hcbs = nullptr;
    page.slots = nullptr;
}

void ObjectHive::push_free(HivePage& page, size_t index, size_t slot_sz)
{
    push_free_slot(page.slots, index, slot_sz, page.free_head);
}

size_t ObjectHive::next_page_capacity() const
{
    return ::velk::next_page_capacity(pages_.size());
}

bool ObjectHive::find_slot(const void* obj, size_t& page_idx, size_t& slot_idx) const
{
    auto obj_addr = reinterpret_cast<uintptr_t>(obj);
    for (size_t pi = 0; pi < pages_.size(); ++pi) {
        auto& page = *pages_[pi];
        auto base = reinterpret_cast<uintptr_t>(page.slots);
        auto end = base + page.capacity * slot_size_;
        if (obj_addr >= base && obj_addr < end) {
            size_t offset = static_cast<size_t>(obj_addr - base);
            if (offset % slot_size_ == 0) {
                size_t si = offset / slot_size_;
                if (page.state[si] == SlotState::Active) {
                    page_idx = pi;
                    slot_idx = si;
                    return true;
                }
            }
        }
    }
    return false;
}

IObject::Ptr ObjectHive::add()
{
    if (!factory_) {
        return {};
    }

    std::lock_guard<std::shared_mutex> lock(mutex_);

    // Check cached page hint first, then scan.
    HivePage* target = nullptr;
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
        alloc_page(next_page_capacity());
        target = pages_.back().get();
    }

    current_page_ = target;

    // Pop slot from freelist.
    size_t slot_idx = pop_free_slot(target->slots, slot_size_, target->free_head);
    target->state[slot_idx] = SlotState::Active;
    ++target->live_count;

    // Set active bit.
    size_t word = slot_idx / 64;
    size_t bit = slot_idx % 64;
    target->active_bits[word] |= uint64_t(1) << bit;

    // Initialize the embedded HiveControlBlock (no heap allocation).
    auto* hcb = &target->hcbs[slot_idx];
    hcb->ecb.strong.store(1, std::memory_order_relaxed);
    hcb->ecb.weak.store(1, std::memory_order_relaxed);
    hcb->ecb.destroy = hive_destroy;
    hcb->ecb.set_ptr(nullptr);
    hcb->page = target;

    // Placement-construct the object, installing the hive's control block.
    // The factory swaps in our block and returns the auto-allocated one to the pool.
    void* slot = slot_ptr(*target, slot_idx);
    auto* obj = factory_->construct_in_place(slot, &hcb->ecb, ObjectFlags::HiveManaged);

    // Set the self-pointer and external + embedded tags on the block.
    hcb->ecb.set_ptr(static_cast<void*>(obj));
    hcb->ecb.set_external_tag();
    hcb->ecb.set_embedded_tag();

    // The hive owns one strong ref (keeps the object alive while in the hive).
    // The returned shared_ptr will acquire a second strong ref via adopt_ref + ref().
    ++live_count_;

    IObject::Ptr result(obj, &hcb->ecb, adopt_ref);
    obj->ref(); // Hive's strong ref
    return result;
}

ReturnValue ObjectHive::remove(IObject& object)
{
    {
        std::lock_guard<std::shared_mutex> lock(mutex_);

        size_t page_idx, slot_idx;
        if (!find_slot(&object, page_idx, slot_idx)) {
            return ReturnValue::Fail;
        }

        // Clear active bit before transitioning to Zombie.
        size_t word = slot_idx / 64;
        size_t bit = slot_idx % 64;
        pages_[page_idx]->active_bits[word] &= ~(uint64_t(1) << bit);

        // Transition Active -> Zombie. The object stays alive until external refs drop.
        // When the last strong ref drops, unref() calls hive_destroy which transitions
        // Zombie -> Free.
        pages_[page_idx]->state[slot_idx] = SlotState::Zombie;
        --live_count_;
    }

    // Release the hive's strong ref outside the lock. If this is the last ref,
    // unref() triggers hive_destroy which re-acquires the lock to reclaim the slot.
    object.unref();

    return ReturnValue::Success;
}

bool ObjectHive::contains(const IObject& object) const
{
    std::shared_lock lock(mutex_);
    size_t page_idx, slot_idx;
    return find_slot(&object, page_idx, slot_idx);
}

/**
 * @brief Shared bitmask scan loop with prefetching.
 *
 * Iterates all active slots across all pages. For each active slot, prefetches
 * the next active slot at (slot_ptr + prefetch_offset), then calls the visitor.
 * The visitor returns false to stop early.
 *
 * @tparam VisitFn bool(IObject* obj, size_t slot_index) - return false to stop.
 */
template <class VisitFn>
void ObjectHive::scan_active(ptrdiff_t prefetch_offset, VisitFn&& visit) const
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
                // Re-check the live bit: a visitor callback may have triggered
                // hive_destroy (via unref) which clears the bit for a slot in
                // this same word.
                if (!(page.active_bits[w] & (uint64_t(1) << b))) {
                    continue;
                }
                // Prefetch next active slot.
                uint64_t remaining = bits;
                if (remaining) {
                    unsigned nb = bitscan_forward64(remaining);
                    prefetch_line(static_cast<char*>(slot_ptr(page, w * 64 + nb)) + prefetch_offset);
                } else if (w + 1 < num_words) {
                    for (size_t nw = w + 1; nw < num_words; ++nw) {
                        uint64_t next_bits = page.active_bits[nw];
                        if (next_bits) {
                            prefetch_line(
                                static_cast<char*>(slot_ptr(page, nw * 64 + bitscan_forward64(next_bits))) +
                                prefetch_offset);
                            break;
                        }
                    }
                }
                if (!visit(slot_ptr(page, i))) {
                    return;
                }
            }
        }
    }
}

void ObjectHive::for_each(void* context, VisitorFn visitor) const
{
    std::shared_lock lock(mutex_);
    scan_active(0, [&](void* slot) { return visitor(context, *static_cast<IObject*>(slot)); });
}

void ObjectHive::for_each_state(ptrdiff_t state_offset, void* context, StateVisitorFn visitor) const
{
    std::shared_lock lock(mutex_);
    scan_active(state_offset, [&](void* slot) {
        auto* obj = static_cast<IObject*>(slot);
        return visitor(context, *obj, reinterpret_cast<char*>(slot) + state_offset);
    });
}

} // namespace velk
