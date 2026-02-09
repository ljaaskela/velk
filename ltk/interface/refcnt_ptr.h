#ifndef REFCNT_PTR_H
#define REFCNT_PTR_H

#include <interface/intf_interface.h>

/**
 * @brief Intrusive reference-counting smart pointer for IInterface-derived types.
 *
 * Calls Ref() on acquisition and UnRef() on release. Unlike std::shared_ptr,
 * the reference count is stored in the object itself.
 *
 * @tparam T An IInterface-derived type.
 */
template<class T>
class refcnt_ptr
{
    static_assert(std::is_convertible_v<T *, IInterface *>, "T must derive from IInterface");
public:
    constexpr refcnt_ptr() = default;
    constexpr refcnt_ptr(T *p) noexcept { reset(p); }
    ~refcnt_ptr() noexcept { release(); }
    constexpr refcnt_ptr &operator=(const refcnt_ptr &o) noexcept
    {
        reset(o.ptr_);
        return *this;
    }
    constexpr refcnt_ptr(const refcnt_ptr &o) noexcept { reset(o.ptr_); }
    constexpr refcnt_ptr(refcnt_ptr &&o) noexcept
    {
        release();
        ptr_ = std::exchange(o.ptr_, nullptr);
    }
    operator bool() const noexcept { return ptr_ != nullptr; }
    T *operator->() noexcept { return ptr_; }
    const T *operator->() const noexcept { return ptr_; }
    T *get() noexcept { return ptr_; }
    const T *get() const noexcept { return ptr_; }
    /** @brief Releases the current pointer and optionally acquires a new one. */
    constexpr void reset(T *ptr = nullptr) noexcept
    {
        release();
        if (ptr) {
            ptr->Ref();
            ptr_ = ptr;
        }
    }

private:
    constexpr void release() noexcept
    {
        if (ptr_) {
            ptr_->UnRef();
            ptr_ = {};
        }
    }
    T *ptr_{};
};

#endif // REFCNT_PTR_H
