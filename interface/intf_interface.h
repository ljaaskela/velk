#ifndef INTF_INTERFACE_H
#define INTF_INTERFACE_H

#include <common.h>

#define NO_COPY_MOVE(name) \
private: \
    name(const name &) = delete; \
    name &operator=(const name &) = delete; \
    name(name &&) = delete; \
    name &operator=(name &&) = delete;

#define INTERFACE(name) \
public: \
    static constexpr Uid UID{MakeInterfaceUid(name)}; \
    using Ptr = std::shared_ptr<name>; \
    using ConstPtr = std::shared_ptr<const name>; \
    using WeakPtr = std::weak_ptr<name>; \
    using ConstWeakPtr = std::weak_ptr<const name>; \
    using IInterface::GetInterface; \
    NO_COPY_MOVE(name) \
\
protected: \
    name() = default; \
    virtual ~name() = default; \
\
private:

class IInterface
{
public:
    static constexpr Uid UID = 0;
    using Ptr = std::shared_ptr<IInterface>;
    using WeakPtr = std::weak_ptr<IInterface>;

public:
    virtual IInterface* GetInterface(Uid uid) = 0;
    virtual const IInterface *GetInterface(Uid uid) const = 0;

    template<class T>
    T *GetInterface() noexcept
    {
        return static_cast<T*>(GetInterface(T::UID));
    }
    template<class T>
    const T *GetInterface() const noexcept
    {
        return static_cast<const T *>(GetInterface(T::UID));
    }

    virtual void Ref() = 0;
    virtual void UnRef() = 0;

protected:
    IInterface() = default;
    virtual ~IInterface() = default;
    NO_COPY_MOVE(IInterface)
};

template<class T, class = std::enable_if_t<std::is_convertible_v<std::decay_t<T *>, IInterface *>>>
class refcnt_ptr
{
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
    constexpr refcnt_ptr(refcnt_ptr &&o) noexcept { ptr_ = std::exchange(o.ptr_, nullptr); }
    operator bool() const noexcept { return ptr_ != nullptr; }
    T *operator->() noexcept { return ptr_; }
    const T *operator->() const noexcept { return ptr_; }
    T *get() noexcept { return ptr_; }
    const T *get() const noexcept { return ptr_; }
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

#endif // INTF_INTERFACE_H
