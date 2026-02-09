#ifndef INTF_INTERFACE_H
#define INTF_INTERFACE_H

#include <common.h>

struct NoCopy
{
    NoCopy() = default;
    ~NoCopy() = default;
    NoCopy(const NoCopy &) = delete;
    NoCopy &operator=(const NoCopy &) = delete;
};

struct NoMove
{
    NoMove() = default;
    ~NoMove() = default;
    NoMove(NoMove &&) = delete;
    NoMove &operator=(NoMove &&) = delete;
};

struct NoCopyMove : public NoCopy, public NoMove
{
    NoCopyMove() = default;
    ~NoCopyMove() = default;
};

class IInterface : NoCopyMove
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
};

template<typename T>
class InterfaceBase : public IInterface
{
public:
    static constexpr Uid UID = TypeUid<T>();
    using Ptr = std::shared_ptr<T>;
    using ConstPtr = std::shared_ptr<const T>;
    using WeakPtr = std::weak_ptr<T>;
    using ConstWeakPtr = std::weak_ptr<const T>;
    using IInterface::GetInterface;

protected:
    InterfaceBase() = default;
    ~InterfaceBase() override = default;
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
