#ifndef VELK_REFCNT_PTR_H
#define VELK_REFCNT_PTR_H

namespace velk {
class IInterface;
}

/**
 * @brief Intrusive reference-counting smart pointer for IInterface-derived types.
 *
 * Calls ref() on acquisition and unref() on release. Unlike std::shared_ptr,
 * the reference count is stored in the object itself.
 *
 * @tparam T An IInterface-derived type.
 */
template <class T>
class refcnt_ptr
{
    static_assert(std::is_convertible_v<T*, IInterface*>, "T must derive from IInterface");

public:
    constexpr refcnt_ptr() = default;
    /** @brief Takes ownership of @p p, calling ref() on it. */
    constexpr refcnt_ptr(T* p) noexcept { reset(p); }
    ~refcnt_ptr() noexcept { release(); }
    constexpr refcnt_ptr& operator=(const refcnt_ptr& o) noexcept
    {
        reset(o.ptr_);
        return *this;
    }
    /** @brief Copy constructor — shares ownership by calling ref(). */
    constexpr refcnt_ptr(const refcnt_ptr& o) noexcept { reset(o.ptr_); }
    /** @brief Move constructor — transfers ownership without touching the reference count. */
    constexpr refcnt_ptr(refcnt_ptr&& o) noexcept
    {
        release();
        ptr_ = o.ptr_;
        o.ptr_ = nullptr;
    }
    /** @brief Returns true if the pointer is non-null. */
    operator bool() const noexcept { return ptr_ != nullptr; }
    /** @brief Dereferences the managed pointer. */
    T* operator->() noexcept { return ptr_; }
    /** @copydoc operator->() */
    const T* operator->() const noexcept { return ptr_; }
    /** @brief Returns the raw managed pointer. */
    T* get() noexcept { return ptr_; }
    /** @copydoc get() */
    const T* get() const noexcept { return ptr_; }
    /** @brief Releases the current pointer and optionally acquires a new one. */
    constexpr void reset(T* ptr = nullptr) noexcept
    {
        release();
        if (ptr) {
            ptr->ref();
            ptr_ = ptr;
        }
    }

private:
    constexpr void release() noexcept
    {
        if (ptr_) {
            ptr_->unref();
            ptr_ = {};
        }
    }
    T* ptr_{};
};

#endif // VELK_REFCNT_PTR_H
