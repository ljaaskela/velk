#ifndef VELK_MEMORY_H
#define VELK_MEMORY_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace velk {

// control_block

/**
 * @brief Shared control block for shared_ptr/weak_ptr.
 *
 * For IInterface types: strong count mirrors the intrusive refCount;
 * block is lazily created when weak references are needed.
 * For non-IInterface types: block is always created and owns the destroy function.
 */
struct control_block
{
    std::atomic<int32_t> strong{0};
    std::atomic<int32_t> weak{1};           // 1 = "strong group exists"
    void (*destroy)(void*){nullptr};        // null for IInterface types (self-deletes via unref)
    void* ptr{nullptr};                     // for destroy call (non-IInterface only)

    /** @brief Increments the strong count. */
    void add_ref() { strong.fetch_add(1, std::memory_order_relaxed); }

    /** @brief Decrements the strong count. Returns true if this was the last strong ref. */
    bool release_ref()
    {
        if (strong.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            return true;
        }
        return false;
    }

    /** @brief CAS loop: increments strong only if > 0. Returns true on success. */
    bool try_add_ref()
    {
        int32_t old = strong.load(std::memory_order_relaxed);
        while (old > 0) {
            if (strong.compare_exchange_weak(old, old + 1,
                    std::memory_order_acquire, std::memory_order_relaxed)) {
                return true;
            }
        }
        return false;
    }

    /** @brief Increments the weak count. */
    void add_weak() { weak.fetch_add(1, std::memory_order_relaxed); }

    /** @brief Decrements the weak count. Deletes block if it reaches zero. */
    void release_weak()
    {
        if (weak.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            delete this;
        }
    }
};

// non-templated helpers

namespace detail {

/** @brief Intrusive ref: increments refCount and mirrors to block. */
inline void intrusive_ref(std::atomic<int32_t>& refCount, control_block* block)
{
    refCount++;
    if (block) block->add_ref();
}

/** @brief Intrusive unref: decrements refCount, returns true if caller should delete itself. */
inline bool intrusive_unref(std::atomic<int32_t>& refCount, control_block* block)
{
    if (block) {
        if (--refCount == 0) {
            block->strong.store(0, std::memory_order_release);
            return true;
        }
        block->release_ref();
    } else if (--refCount == 0) {
        return true;
    }
    return false;
}

/** @brief Called after delete this to release the "strong group" weak ref. */
inline void intrusive_post_delete(control_block* block)
{
    if (block) block->release_weak();
}

/** @brief Lazily creates a control block, syncing its strong count with refCount. */
inline void ensure_block(std::atomic<int32_t>& refCount, control_block*& block)
{
    if (!block) {
        block = new control_block();
        block->strong.store(refCount.load(std::memory_order_relaxed),
                            std::memory_order_relaxed);
    }
}

/** @brief Shared acquire for IInterface types: add weak ref only. */
inline void shared_acquire_intrusive(control_block* block)
{
    if (block) block->add_weak();
}

/** @brief Shared acquire for non-IInterface types: add strong + weak ref. */
inline void shared_acquire_external(control_block* block)
{
    if (!block) return;
    block->add_ref();
    block->add_weak();
}

/** @brief Shared release for IInterface types: release weak ref only (unref handles strong). */
inline void shared_release_intrusive(control_block* block)
{
    if (block) block->release_weak();
}

/** @brief Shared release for non-IInterface types: release strong + destroy + weak. */
inline void shared_release_external(control_block* block)
{
    if (!block) return;
    if (block->release_ref()) {
        block->destroy(block->ptr);
    }
    block->release_weak();
}

} // namespace detail

// adopt_ref tag

/** @brief Tag type for adopting an existing reference without incrementing. */
struct adopt_ref_t {};
inline constexpr adopt_ref_t adopt_ref{};

// forward declarations

class IInterface; // forward declaration

template<class T> class shared_ptr;
template<class T> class weak_ptr;

// ptr_base

/**
 * @brief Common base for shared_ptr and weak_ptr holding shared members.
 */
template<class T>
class ptr_base
{
    template<class U> friend class ptr_base;
    template<class U> friend class shared_ptr;
    template<class U> friend class weak_ptr;

protected:
    static constexpr bool is_interface =
        std::is_convertible_v<std::remove_const_t<T>*, IInterface*>;
    using mutable_t = std::remove_const_t<T>;

    constexpr ptr_base() = default;
    constexpr ptr_base(T* p, control_block* b) : ptr_(p), block_(b) {}

    /** @brief Returns a non-const pointer for ref()/unref() calls on const T. */
    mutable_t* mutable_ptr() const { return const_cast<mutable_t*>(ptr_); }

    void swap(ptr_base& o) noexcept
    {
        std::swap(ptr_, o.ptr_);
        std::swap(block_, o.block_);
    }

    T* ptr_{};
    control_block* block_{};
};

// shared_ptr

/**
 * @brief ABI-stable shared pointer.
 *
 * For IInterface-derived types: uses intrusive ref/unref, block obtained from object.
 * For non-IInterface types: uses external control block with type-erased destructor.
 */
template<class T>
class shared_ptr : public ptr_base<T>
{
    using base = ptr_base<T>;
    using base::is_interface;
    using base::ptr_;
    using base::block_;

    template<class U> friend class shared_ptr;
    template<class U> friend class weak_ptr;

    /** @brief Increments strong + weak refs for a non-null ptr_. */
    void acquire()
    {
        if (!ptr_) return;
        if constexpr (is_interface) {
            this->mutable_ptr()->ref();
            detail::shared_acquire_intrusive(block_);
        } else {
            detail::shared_acquire_external(block_);
        }
    }

    void release()
    {
        if (!ptr_) return;
        if constexpr (is_interface) {
            this->mutable_ptr()->unref();
            detail::shared_release_intrusive(block_);
        } else {
            detail::shared_release_external(block_);
        }
        ptr_ = nullptr;
        block_ = nullptr;
    }

public:
    constexpr shared_ptr() = default;
    constexpr shared_ptr(std::nullptr_t) {}

    /** @brief Constructs from raw pointer. Adopts the existing reference (IInterface)
     *  or creates a new control block (non-IInterface). */
    explicit shared_ptr(T* p)
    {
        if (!p) return;
        ptr_ = p;
        if constexpr (!is_interface) {
            block_ = new control_block();
            block_->strong.store(1, std::memory_order_relaxed);
            block_->ptr = p;
            block_->destroy = [](void* obj) { delete static_cast<T*>(obj); };
        }
    }

    /** @brief Constructs from raw pointer + existing block (aliasing/interface cast). Adds ref. */
    shared_ptr(T* p, control_block* b) : base(p, b) { acquire(); }

    /** @brief Adopts an existing reference without incrementing ref count. */
    shared_ptr(T* p, control_block* b, adopt_ref_t) : base(p, b)
    {
        if (block_) block_->add_weak();
    }

    ~shared_ptr() { release(); }

    shared_ptr(const shared_ptr& o) : base(o.ptr_, o.block_) { acquire(); }

    shared_ptr& operator=(const shared_ptr& o)
    {
        shared_ptr(o).swap(*this);
        return *this;
    }

    shared_ptr(shared_ptr&& o) noexcept : base(o.ptr_, o.block_)
    {
        o.ptr_ = nullptr;
        o.block_ = nullptr;
    }

    shared_ptr& operator=(shared_ptr&& o) noexcept
    {
        shared_ptr(std::move(o)).swap(*this);
        return *this;
    }

    /** @brief Converting constructor: shared_ptr<U> -> shared_ptr<T> */
    template<class U, class = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    shared_ptr(const shared_ptr<U>& o) : base(o.ptr_, o.block_) { acquire(); }

    /** @brief Converting move constructor: shared_ptr<U> -> shared_ptr<T> */
    template<class U, class = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    shared_ptr(shared_ptr<U>&& o) noexcept : base(o.ptr_, o.block_)
    {
        o.ptr_ = nullptr;
        o.block_ = nullptr;
    }

    T* operator->() const { return ptr_; }
    T& operator*() const { return *ptr_; }
    T* get() const { return ptr_; }

    explicit operator bool() const { return ptr_ != nullptr; }

    control_block* block() const { return block_; }

    void reset()
    {
        release();
        ptr_ = nullptr;
        block_ = nullptr;
    }

    /** @brief Sets the control block (used by ObjectCore after construction). */
    void set_block(control_block* b)
    {
        if (block_) block_->release_weak();
        block_ = b;
        if (block_) block_->add_weak();
    }

    bool operator==(const shared_ptr& o) const { return ptr_ == o.ptr_; }
    bool operator!=(const shared_ptr& o) const { return ptr_ != o.ptr_; }
    bool operator==(std::nullptr_t) const { return ptr_ == nullptr; }
    bool operator!=(std::nullptr_t) const { return ptr_ != nullptr; }

    template<class U>
    bool operator==(const shared_ptr<U>& o) const { return ptr_ == o.get(); }
    template<class U>
    bool operator!=(const shared_ptr<U>& o) const { return ptr_ != o.get(); }
};

// weak_ptr

/**
 * @brief ABI-stable weak pointer. Pairs with shared_ptr via control_block.
 */
template<class T>
class weak_ptr : public ptr_base<T>
{
    using base = ptr_base<T>;
    using base::is_interface;
    using base::ptr_;
    using base::block_;

    template<class U> friend class weak_ptr;
    template<class U> friend class shared_ptr;

    void release()
    {
        if (block_) block_->release_weak();
        ptr_ = nullptr;
        block_ = nullptr;
    }

public:
    constexpr weak_ptr() = default;

    weak_ptr(const shared_ptr<T>& sp) : base(sp.ptr_, sp.block_)
    {
        if (block_) block_->add_weak();
    }

    ~weak_ptr() { release(); }

    weak_ptr(const weak_ptr& o) : base(o.ptr_, o.block_)
    {
        if (block_) block_->add_weak();
    }

    weak_ptr& operator=(const weak_ptr& o)
    {
        weak_ptr(o).swap(*this);
        return *this;
    }

    weak_ptr& operator=(const shared_ptr<T>& sp)
    {
        weak_ptr(sp).swap(*this);
        return *this;
    }

    weak_ptr(weak_ptr&& o) noexcept : base(o.ptr_, o.block_)
    {
        o.ptr_ = nullptr;
        o.block_ = nullptr;
    }

    weak_ptr& operator=(weak_ptr&& o) noexcept
    {
        weak_ptr(std::move(o)).swap(*this);
        return *this;
    }

    /** @brief Attempts to acquire a shared_ptr. Returns empty if object is gone. */
    shared_ptr<T> lock() const
    {
        if (!block_ || !block_->try_add_ref()) {
            return {};
        }
        // try_add_ref succeeded — we now own one strong ref in the block.
        // Construct a shared_ptr that adopts this ref.
        shared_ptr<T> result;
        result.ptr_ = ptr_;
        result.block_ = block_;
        if constexpr (is_interface) {
            this->mutable_ptr()->ref();
        }
        block_->add_weak();
        return result;
    }

    /** @brief Returns true if the object has been destroyed. */
    bool expired() const
    {
        return !block_ || block_->strong.load(std::memory_order_acquire) == 0;
    }
};

// make_shared

/** @brief Creates a shared_ptr for a newly constructed object. */
template<class T, class... Args>
shared_ptr<T> make_shared(Args&&... args)
{
    return shared_ptr<T>(new T(std::forward<Args>(args)...));
}

} // namespace velk

#endif // VELK_MEMORY_H
