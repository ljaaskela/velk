#ifndef API_ANY_H
#define API_ANY_H

#include <cassert>
#include <common.h>
#include <interface/intf_any.h>
#include <interface/intf_registry.h>

// Wrapper for IAny::Ptrs
class ConstAny
{
public:
    operator const IAny *() const noexcept { return any_.get(); }
    const IAny *GetAnyInterface() const noexcept { return any_.get(); }
    operator bool() const noexcept { return any_.operator bool(); }

protected:
    void SetAny(const IAny &any) { any_ = refcnt_ptr<IAny>(const_cast<IAny *>(&any)); }
    void SetAny(const IAny::ConstPtr &any)
    {
        if (any) {
            SetAny(*(any.get()));
        }
    }
    ConstAny() = default;
    virtual ~ConstAny() = default;
    refcnt_ptr<IAny> any_;
};

class Any : public ConstAny
{
public:
    operator IAny *() { return any_.get(); }
    const IAny *GetAnyInterface() { return any_.get(); }
    bool CopyFrom(const IAny &other) { return any_ && any_->CopyFrom(other); }

protected:
    Any() = default;
};

template<class T>
class AnyT final : public Any
{
    static constexpr bool IsReadWrite = !std::is_const_v<T>;
    static constexpr auto TYPE_SIZE = sizeof(T);

public:
    static constexpr Uid TYPE_UID = TypeUid<std::remove_const_t<T>>();

    template<class Flag = std::enable_if_t<IsReadWrite>>
    constexpr AnyT(const IAny::Ptr &any) noexcept
    {
        if (IsCompatible(any, TYPE_UID)) {
            SetAny(any);
        }
    }
    constexpr AnyT(const IAny::ConstPtr &any) noexcept
    {
        if (IsCompatible(any, TYPE_UID)) {
            SetAny(any);
        }
    }
    constexpr AnyT(const IAny &any) noexcept
    {
        if (IsCompatible(any, TYPE_UID)) {
            SetAny(any);
        }
    }
    constexpr AnyT(IAny &&any) noexcept
    {
        if (IsCompatible(any, TYPE_UID)) {
            any_ = std::move(any);
        }
    }
    AnyT() noexcept { Create(); }
    AnyT(const T &value) noexcept
    {
        if (auto any = GetRegistry().CreateAny(TYPE_UID)) {
            any->SetData(&value, TYPE_SIZE, TYPE_UID);
            any_.reset(any.get());
        }
    }
    operator const IAny &() const noexcept { return *(any_.get()); }

    constexpr Uid GetTypeUid() const noexcept { return TYPE_UID; }
    T Get() const noexcept
    {
        std::remove_const_t<T> value{};
        if (any_) {
            any_->GetData(&value, sizeof(T), TYPE_UID);
        }
        return value;
    }
    void Set(const T &value) noexcept
    {
        if (any_) {
            any_->SetData(&value, sizeof(T), TYPE_UID);
        }
    }

    static AnyT<T> Ref(const IAny::Ptr &ref) { return AnyT<T>(ref); }
    static const AnyT<const T> ConstRef(const IAny::ConstPtr &ref) { return AnyT<const T>(ref); }

protected:
    void Create() { SetAny(GetRegistry().CreateAny(TYPE_UID)); }
};

#endif // ANY_H
