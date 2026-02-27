#ifndef VELK_EXT_REFCOUNTED_DISPATCH_H
#define VELK_EXT_REFCOUNTED_DISPATCH_H

#include <velk/ext/interface_dispatch.h>
#include <velk/interface/types.h>
#include <velk/memory.h>

namespace velk {

namespace detail {

/** @brief Marks a ref-counted object as dead and releases its control block. */
inline void release_ref_counted(control_block& block)
{
    block.strong.store(0, std::memory_order_release);
    release_control_block(block);
}

struct BlockAccess;

} // namespace detail

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
            if (data_.block->is_external()) {
                auto* ecb = static_cast<external_control_block*>(data_.block);
                ecb->destroy(ecb);
            } else {
                delete this;
            }
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
    ~RefCountedDispatch() override { detail::release_ref_counted(*data_.block); }

protected:
    /** @brief Per-object data: flags and control block pointer. */
    struct ObjectData
    {
        control_block* block{detail::alloc_control_block()}; ///< Pooled control block (strong=1).
        uint32_t flags{ObjectFlags::None};                   ///< Bitwise combination of ObjectFlags.
    };
    /** @brief Returns a mutable reference to the per-object data. */
    constexpr ObjectData& get_object_data() noexcept { return data_; }
    /** @brief Returns a const reference to the per-object data. */
    constexpr const ObjectData& get_object_data() const noexcept { return data_; }
    /** @brief Returns the control block for shared_ptr/weak_ptr support. */
    control_block* get_block() const noexcept { return data_.block; }

private:
    friend struct detail::BlockAccess;

    /** @brief Replaces the control block. Used internally by placement storage. */
    void replace_block(control_block* block) noexcept { data_.block = block; }

    ObjectData data_;
};

} // namespace ext

namespace detail {

/** @brief Grants access to replace_block() for placement storage and make_object(). */
struct BlockAccess
{
    template <class T>
    static control_block* get(T& obj) noexcept
    {
        return obj.get_block();
    }

    template <class T>
    static void replace(T& obj, control_block* block) noexcept
    {
        obj.replace_block(block);
    }

    template <class T>
    static void set_flags(T& obj, uint32_t flags) noexcept
    {
        obj.get_object_data().flags = flags;
    }
};

} // namespace detail
} // namespace velk

#endif // VELK_EXT_REFCOUNTED_DISPATCH_H
