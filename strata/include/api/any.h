#ifndef API_ANY_H
#define API_ANY_H

#include <api/strata.h>
#include <cassert>
#include <common.h>
#include <interface/intf_any.h>
#include <interface/intf_strata.h>

namespace strata {

/** @brief Read-only wrapper around an IAny pointer with reference-counted ownership. */
class ConstAny
{
public:
    operator const IAny *() const noexcept { return any_.get(); }
    const IAny *GetAnyInterface() const noexcept { return any_.get(); }
    operator bool() const noexcept { return any_.operator bool(); }

protected:
    ConstAny() = default;
    virtual ~ConstAny() = default;

    void SetAny(const IAny &any, const Uid &req) noexcept
    {
        if (IsCompatible(any, req)) {
            SetAnyDirect(any);
        }
    }
    void SetAny(const IAny::ConstPtr &any, const Uid &req) noexcept
    {
        if (IsCompatible(any, req)) {
            SetAnyDirect(any);
        }
    }
    void SetAnyDirect(const IAny &any) noexcept
    {
        any_ = refcnt_ptr<IAny>(const_cast<IAny *>(&any));
    }
    void SetAnyDirect(const IAny::ConstPtr &any) noexcept { SetAnyDirect(*(any.get())); }
    refcnt_ptr<IAny> any_;
};

/** @brief Read-write wrapper around an IAny pointer. */
class Any : public ConstAny
{
public:
    operator IAny *() { return any_.get(); }
    bool CopyFrom(const IAny &other) { return any_ && any_->CopyFrom(other); }
    IAny *GetAnyInterface() { return any_.get(); }

protected:
    Any() = default;
};

/**
 * @brief Typed wrapper for IAny that provides Get/Set accessors for type T.
 *
 * Can be constructed from an existing IAny or will create a new one from Strata.
 *
 * @tparam T The value type. Use const T for read-only access.
 */
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
        SetAny(any, TYPE_UID);
    }
    constexpr AnyT(const IAny::ConstPtr &any) noexcept { SetAny(any, TYPE_UID); }
    constexpr AnyT(const IAny &any) noexcept { SetAny(any, TYPE_UID); }
    constexpr AnyT(IAny &&any) noexcept
    {
        if (IsCompatible(any, TYPE_UID)) {
            any_ = std::move(any);
        }
    }
    AnyT() noexcept { Create(); }
    AnyT(const T &value) noexcept
    {
        if (!IsCompatible(any_, TYPE_UID)) {
            auto any = Strata().CreateAny(TYPE_UID);
            any_.reset(any.get());
        }
        any_->SetData(&value, TYPE_SIZE, TYPE_UID);
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
    void Create() { SetAnyDirect(Strata().CreateAny(TYPE_UID)); }
};

} // namespace strata

#endif // ANY_H
