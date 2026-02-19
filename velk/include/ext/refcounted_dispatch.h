#ifndef EXT_REFCOUNTED_DISPATCH_H
#define EXT_REFCOUNTED_DISPATCH_H

#include <atomic>
#include <memory.h>
#include <ext/interface_dispatch.h>
#include <interface/types.h>

namespace velk {

namespace ext {
/**
 * @brief Adds intrusive reference counting to InterfaceDispatch.
 *
 * @tparam Interfaces The interfaces this object implements.
 */
template<class... Interfaces>
class RefCountedDispatch : public InterfaceDispatch<Interfaces...>
{
public:
    /** @brief Atomically increments the reference count. */
    void ref() override { detail::intrusive_ref(data_.refCount, *data_.block); }

    /** @brief Atomically decrements the reference count; deletes the object at zero. */
    void unref() override
    {
        auto& block = *data_.block;
        if (detail::intrusive_unref(data_.refCount, block)) {
            delete this;
            detail::intrusive_post_delete(block);
        }
    }

public:
    RefCountedDispatch() { data_.block->strong.store(1, std::memory_order_relaxed); }
    ~RefCountedDispatch() override = default;

public:
    /** @brief Returns the control block for shared_ptr/weak_ptr support. */
    control_block* get_block() const noexcept { return data_.block; }

protected:
    struct ObjectData
    {
        alignas(4) std::atomic<int32_t> refCount{1};
        alignas(4) int32_t flags{ObjectFlags::None};
        control_block* block{new control_block()};
    };
    constexpr ObjectData &get_object_data() noexcept { return data_; }

private:
    ObjectData data_;
};

} // namespace ext
} // namespace velk

#endif // EXT_REFCOUNTED_DISPATCH_H
