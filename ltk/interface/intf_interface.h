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

#include <interface/refcnt_ptr.h>

template<typename T>
class Interface : public IInterface
{
public:
    static constexpr Uid UID = TypeUid<T>();
    using Ptr = std::shared_ptr<T>;
    using ConstPtr = std::shared_ptr<const T>;
    using WeakPtr = std::weak_ptr<T>;
    using ConstWeakPtr = std::weak_ptr<const T>;
    using RefPtr = refcnt_ptr<T>;
    using IInterface::GetInterface;

protected:
    Interface() = default;
    ~Interface() override = default;
};

#endif // INTF_INTERFACE_H
