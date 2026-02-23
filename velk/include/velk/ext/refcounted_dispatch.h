#ifndef VELK_EXT_REFCOUNTED_DISPATCH_H
#define VELK_EXT_REFCOUNTED_DISPATCH_H

#include <velk/ext/interface_dispatch.h>
#include <velk/interface/types.h>
#include <velk/memory.h>

namespace velk {

namespace ext {
/**
 * @brief Adds intrusive reference counting to InterfaceDispatch.
 *
 * The reference count lives in the heap-allocated control_block (strong count).
 * This avoids a redundant per-object atomic and keeps the count accessible
 * to shared_ptr/weak_ptr without indirection.
 *
 * @tparam Interfaces The interfaces this object implements.
 */
template <class... Interfaces>
class RefCountedDispatch : public InterfaceDispatch<Interfaces...>
{
public:
    /** @brief Atomically increments the reference count. */
    void ref() override { data_.block->add_ref(); }

    /** @brief Atomically decrements the reference count; deletes the object at zero. */
    void unref() override
    {
        if (data_.block->release_ref()) {
            delete this;
        }
    }

public:
    RefCountedDispatch() = default;

    /**
     * @brief Marks the object as dead and releases the control block.
     *
     * Zeroes the strong count so that any outstanding weak_ptr sees the object
     * as expired, then releases the "strong group" weak ref on the block.
     */
    ~RefCountedDispatch() override
    {
        data_.block->strong.store(0, std::memory_order_release);
        detail::release_control_block(*data_.block);
    }

public:
    /** @brief Returns the control block for shared_ptr/weak_ptr support. */
    control_block* get_block() const noexcept { return data_.block; }

protected:
    /** @brief Per-object data: flags and control block pointer. */
    struct ObjectData
    {
        control_block* block{detail::alloc_control_block()}; ///< Pooled control block (strong=1).
        int32_t flags{ObjectFlags::None};                    ///< Bitwise combination of ObjectFlags.
    };
    /** @brief Returns a mutable reference to the per-object data. */
    constexpr ObjectData& get_object_data() noexcept { return data_; }

private:
    ObjectData data_;
};

} // namespace ext
} // namespace velk

#endif // VELK_EXT_REFCOUNTED_DISPATCH_H
