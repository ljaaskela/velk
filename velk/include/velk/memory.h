#ifndef VELK_MEMORY_H
#define VELK_MEMORY_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>
#include <velk/velk_export.h>

namespace velk {

/**
 * @brief Shared control block for shared_ptr/weak_ptr (16 bytes).
 *
 * For IInterface types (intrusive mode): @c strong is the authoritative
 * reference count for the object, and @c ptr stores the IObject*
 * self-pointer used by IObject::get_self().
 *
 * For non-IInterface types: use external_control_block, which extends
 * this with a type-erased destroy function.
 *
 * @par Weak count semantics
 * The @c weak count starts at 1, representing the "strong group is alive"
 * reference. Each weak_ptr adds +1 on construction and -1 on destruction.
 * When the last strong ref drops, the strong group's +1 is released. The
 * block is deleted when weak reaches zero.
 */
struct control_block
{
    std::atomic<int32_t> strong{0};
    std::atomic<int32_t> weak{1};           ///< 1 = "strong group exists"
    void* ptr{nullptr};                     ///< IObject* (IInterface) or destroy target (non-IInterface)

    /** @brief Increments the strong count (relaxed). */
    void add_ref() {
        strong.fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * @brief Decrements the strong count (acq_rel).
     * @return true if this was the last strong ref.
     */
    bool release_ref()
    {
        if (strong.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            return true;
        }
        return false;
    }

    /**
     * @brief Attempts to increment the strong count only if it is currently > 0.
     *
     * Used by weak_ptr::lock() to atomically promote a weak reference to a
     * strong one. Implemented as a CAS loop.
     *
     * @return true if the strong count was successfully incremented.
     */
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

    /** @brief Increments the weak count (relaxed). */
    void add_weak() {
        weak.fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * @brief Decrements the weak count (acq_rel).
     * @return true if this was the last weak ref (caller must deallocate the block).
     */
    bool release_weak()
    {
        return weak.fetch_sub(1, std::memory_order_acq_rel) == 1;
    }
};

/**
 * @brief Extended control block for non-IInterface types (24 bytes).
 *
 * Adds a type-erased destroy function pointer. When the last strong ref
 * drops, @c destroy is called with @c ptr to delete the owned object.
 */
struct external_control_block : control_block
{
    void (*destroy)(void*){nullptr};        ///< Type-erased destructor for the owned object
};

namespace detail {

/**
 * @brief Allocates a control_block, reusing a pooled block when available.
 *
 * Implemented in velk.cpp to keep all heap operations within the DLL,
 * avoiding cross-module new/delete mismatches.
 *
 * @param external If true, allocates an external_control_block (24 bytes,
 *                 not pooled). If false, allocates a control_block (16 bytes,
 *                 pooled when available).
 * @return A block initialized with strong=1, weak=1, ptr=nullptr
 *         (and destroy=nullptr for external blocks).
 */
VELK_EXPORT control_block* alloc_control_block(bool external = false);

/**
 * @brief Returns a control_block to the pool, or frees it if the pool is full.
 *
 * @param block The control block to recycle.
 * @param external Must match the value passed to alloc_control_block.
 *                 External blocks are deleted as external_control_block
 *                 (required since control_block has no virtual destructor).
 */
VELK_EXPORT void dealloc_control_block(control_block* block, bool external = false);

/**
 * @brief Releases the "strong group" weak ref, freeing the block if no weak_ptrs remain.
 *
 * Called from ~RefCountedDispatch(). The block is heap-allocated (not embedded
 * in the object), so it safely outlives the destroyed object when weak_ptrs
 * still hold references.
 *
 * @param block The control block.
 */
inline void release_control_block(control_block& block)
{
    if (block.release_weak()) {
        dealloc_control_block(&block);
    }
}

/**
 * @brief Releases one weak ref on a pooled control_block (IInterface types).
 *
 * Used for both shared and weak pointer release on intrusive types, since
 * strong count management is handled by ref()/unref() on the object itself.
 *
 * @param block The control block, or nullptr.
 */
inline void weak_release_intrusive(control_block* block)
{
    if (block && block->release_weak()) {
        dealloc_control_block(block);
    }
}

/**
 * @brief Releases one weak ref on an external_control_block (non-IInterface types).
 * @param block The control block, or nullptr.
 */
inline void weak_release_external(control_block* block)
{
    if (block && block->release_weak()) {
        dealloc_control_block(block, true);
    }
}

/**
 * @brief Releases a shared reference for non-IInterface types.
 *
 * Decrements the strong count; if it reaches zero, calls the type-erased
 * destroy function. Then decrements the weak count; if it reaches zero,
 * frees the external_control_block via the DLL.
 *
 * @param block The control block, or nullptr.
 */
inline void shared_release_external(control_block* block)
{
    if (block) {
        auto* ecb = static_cast<external_control_block*>(block);
        if (block->release_ref()) {
            ecb->destroy(block->ptr);
        }
        if (block->release_weak()) {
            dealloc_control_block(block, true);
        }
    }
}

} // namespace detail

/** @brief Tag type for adopting an existing reference without incrementing. */
struct adopt_ref_t {};
/** @brief Tag value for adopt_ref_t. */
inline constexpr adopt_ref_t adopt_ref{};

class IInterface;

template<class T> class shared_ptr;
template<class T> class weak_ptr;

/**
 * @brief Common base for shared_ptr and weak_ptr.
 *
 * Holds the raw pointer and control block pointer shared by both smart
 * pointer types. Provides swap, release_block, and a const_cast helper
 * for calling ref()/unref() through const pointers.
 *
 * @tparam T The pointed-to type.
 */
template<class T>
class ptr_base
{
    template<class U> friend class ptr_base;
    template<class U> friend class shared_ptr;
    template<class U> friend class weak_ptr;

protected:
    /// True if T derives from IInterface (selects intrusive vs external mode).
    static constexpr bool is_interface =
        std::is_convertible_v<std::remove_const_t<T>*, IInterface*>;
    using mutable_t = std::remove_const_t<T>;

    constexpr ptr_base() = default;
    constexpr ptr_base(T* p, control_block* b) : ptr_(p), block_(b) {}

    /** @brief Returns a non-const pointer for ref()/unref() calls on const T. */
    mutable_t* mutable_ptr() const { return const_cast<mutable_t*>(ptr_); }

    /** @brief Swaps pointer and block with another ptr_base. */
    void swap(ptr_base& o) noexcept
    {
        std::swap(ptr_, o.ptr_);
        std::swap(block_, o.block_);
    }

    /** @brief Releases the weak ref on the block, using the correct deallocator for the block type. */
    void release_block()
    {
        if constexpr (is_interface) {
            detail::weak_release_intrusive(block_);
        } else {
            detail::weak_release_external(block_);
        }
    }

    T* ptr_{};              ///< Raw pointer to the managed object
    control_block* block_{};///< Associated control block (may be null for IInterface raw-ptr construction)
};

/**
 * @brief ABI-stable shared ownership pointer ({T*, control_block*} = 16 bytes).
 *
 * Operates in two modes selected at compile time via @c is_interface:
 *
 * **Intrusive mode** (IInterface-derived T): calls ref()/unref() on the
 * object for strong count management. The control block only tracks the
 * weak count and stores the IObject* self-pointer. Multiple independent
 * shared_ptr instances can be created from raw pointers to the same object.
 *
 * **External mode** (non-IInterface T): allocates an external_control_block
 * with a type-erased destructor, similar to std::shared_ptr. The control
 * block owns both the strong and weak counts.
 *
 * @tparam T The pointed-to type.
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
            if (block_) block_->add_weak();
        } else if (block_) {
            block_->add_ref();
            block_->add_weak();
        }
    }

    /** @brief Decrements strong + weak refs and nulls out the pointer. */
    void release()
    {
        if (!ptr_) return;
        if constexpr (is_interface) {
            this->mutable_ptr()->unref();
            detail::weak_release_intrusive(block_);
        } else {
            detail::shared_release_external(block_);
        }
        ptr_ = nullptr;
        block_ = nullptr;
    }

public:
    constexpr shared_ptr() = default;
    constexpr shared_ptr(std::nullptr_t) {}

    /**
     * @brief Constructs from a raw pointer.
     *
     * For IInterface types: adopts the existing intrusive reference (no block).
     * For non-IInterface types: allocates an external_control_block with a
     * type-erased destructor.
     *
     * @param p Raw pointer to manage (may be null).
     */
    explicit shared_ptr(T* p)
    {
        if (!p) return;
        ptr_ = p;
        if constexpr (!is_interface) {
            auto* ecb = static_cast<external_control_block*>(
                detail::alloc_control_block(true));
            ecb->ptr = p;
            ecb->destroy = [](void* obj) { delete static_cast<T*>(obj); };
            block_ = ecb;
        }
    }

    /**
     * @brief Constructs from a raw pointer and existing block (aliasing/interface cast).
     *
     * Adds a reference. Used by interface_pointer_cast and similar.
     */
    shared_ptr(T* p, control_block* b) : base(p, b) { acquire(); }

    /**
     * @brief Adopts an existing reference without incrementing the strong count.
     *
     * Only adds a weak ref to the block. Used by make_object() after construction
     * when the ref count is already 1.
     */
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

    /** @brief Converting copy constructor: shared_ptr<U> to shared_ptr<T>. */
    template<class U, class = std::enable_if_t<std::is_convertible_v<U*, T*>>>
    shared_ptr(const shared_ptr<U>& o) : base(o.ptr_, o.block_) { acquire(); }

    /** @brief Converting move constructor: shared_ptr<U> to shared_ptr<T>. */
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

    /** @brief Returns the underlying control block. */
    control_block* block() const { return block_; }

    /** @brief Releases ownership and resets to null. */
    void reset()
    {
        release();
        ptr_ = nullptr;
        block_ = nullptr;
    }

    /**
     * @brief Replaces the control block.
     *
     * Releases the old block's weak ref (if any) and acquires a weak ref
     * on the new block. Used by ObjectCore after construction.
     */
    void set_block(control_block* b)
    {
        if (block_) {
            this->release_block();
        }
        block_ = b;
        if (block_) {
            block_->add_weak();
        }
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

/**
 * @brief ABI-stable weak pointer ({T*, control_block*} = 16 bytes).
 *
 * Observes an object managed by shared_ptr without preventing destruction.
 * Use lock() to attempt promotion to a shared_ptr.
 *
 * @tparam T The pointed-to type.
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

    /** @brief Releases the weak ref and nulls out the pointer. */
    void release()
    {
        if (block_) {
            this->release_block();
        }
        ptr_ = nullptr;
        block_ = nullptr;
    }

public:
    constexpr weak_ptr() = default;

    /** @brief Constructs a weak_ptr observing the same object as @p sp. */
    weak_ptr(const shared_ptr<T>& sp) : base(sp.ptr_, sp.block_)
    {
        if (block_) block_->add_weak();
    }

    ~weak_ptr() { release(); }

    weak_ptr(const weak_ptr& o) : base(o.ptr_, o.block_)
    {
        if (block_) {
            block_->add_weak();
        }
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

    /**
     * @brief Attempts to promote to a shared_ptr.
     *
     * Uses try_add_ref() to atomically increment the strong count only if
     * the object is still alive. Returns an empty shared_ptr if the object
     * has been destroyed.
     *
     * @return A shared_ptr owning the object, or empty if expired.
     */
    shared_ptr<T> lock() const
    {
        if (!(block_ && block_->try_add_ref())) {
            return {};
        }
        // try_add_ref succeeded: we now own one strong ref in the block.
        // Construct a shared_ptr that adopts this ref (no extra ref() needed
        // for intrusive types since try_add_ref already incremented strong).
        shared_ptr<T> result;
        result.ptr_ = ptr_;
        result.block_ = block_;
        block_->add_weak();
        return result;
    }

    /**
     * @brief Returns true if the object has been destroyed.
     *
     * Checks whether the strong count has dropped to zero.
     */
    bool expired() const
    {
        return !block_ || block_->strong.load(std::memory_order_acquire) == 0;
    }
};

/**
 * @brief Creates a shared_ptr managing a newly constructed T.
 * @tparam T The type to construct.
 * @tparam Args Constructor argument types.
 * @param args Arguments forwarded to T's constructor.
 * @return A shared_ptr owning the new object.
 */
template<class T, class... Args>
shared_ptr<T> make_shared(Args&&... args)
{
    return shared_ptr<T>(new T(std::forward<Args>(args)...));
}

} // namespace velk

#endif // VELK_MEMORY_H
