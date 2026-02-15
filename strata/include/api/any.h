#ifndef API_ANY_H
#define API_ANY_H

#include <api/strata.h>
#include <cassert>
#include <common.h>
#include <type_traits>
#include <interface/intf_any.h>
#include <interface/intf_strata.h>

namespace strata {

/** @brief Read-only wrapper around an IAny pointer with reference-counted ownership. */
class ConstAny
{
public:
    /** @brief Implicit conversion to a const IAny pointer. */
    operator const IAny *() const noexcept { return any_.get(); }
    /** @brief Returns the underlying const IAny pointer. */
    const IAny *get_any_interface() const noexcept { return any_.get(); }
    /** @brief Returns true if the wrapper holds a valid IAny. */
    operator bool() const noexcept { return any_.operator bool(); }

protected:
    ConstAny() = default;
    virtual ~ConstAny() = default;

    void set_any(const IAny &any, const Uid &req) noexcept
    {
        if (is_compatible(any, req)) {
            set_any_direct(any);
        }
    }
    void set_any(const IAny::ConstPtr &any, const Uid &req) noexcept
    {
        if (is_compatible(any, req)) {
            set_any_direct(any);
        }
    }
    void set_any_direct(const IAny &any) noexcept
    {
        any_ = refcnt_ptr<IAny>(const_cast<IAny *>(&any));
    }
    void set_any_direct(const IAny::ConstPtr &any) noexcept { set_any_direct(*(any.get())); }
    refcnt_ptr<IAny> any_;
};

/** @brief Read-write wrapper around an IAny pointer. */
class AnyBase : public ConstAny
{
public:
    /** @brief Implicit conversion to a mutable IAny pointer. */
    operator IAny *() { return any_.get(); }
    /** @brief Copies the value from @p other into the managed IAny. */
    bool copy_from(const IAny &other) { return any_ && any_->copy_from(other); }
    /** @brief Returns the underlying mutable IAny pointer. */
    IAny *get_any_interface() { return any_.get(); }

protected:
    AnyBase() = default;
};

/**
 * @brief Typed wrapper for IAny that provides get_value/set_value accessors for type T.
 *
 * Can be constructed from an existing IAny or will create a new one from Strata.
 *
 * @tparam T The value type. Use const T for read-only access.
 */
template<class T>
class Any final : public AnyBase
{
    static constexpr bool IsReadWrite = !std::is_const_v<T>;
    static constexpr bool IsReadOnly = !IsReadWrite;
    static constexpr auto TYPE_SIZE = sizeof(T);

public:
    static constexpr Uid TYPE_UID = type_uid<std::remove_const_t<T>>();

    /** @brief Wraps an existing mutable IAny pointer (read-write). */
    template<class Flag = std::enable_if_t<IsReadWrite>>
    constexpr Any(const IAny::Ptr &any) noexcept
    {
        set_any(any, TYPE_UID);
    }
    /** @brief Wraps an existing const IAny pointer (read-only). */
    template<class Flag = std::enable_if_t<IsReadOnly>>
    constexpr Any(const IAny::ConstPtr &any) noexcept
    {
        set_any(any, TYPE_UID);
    }
    /** @brief Wraps a const IAny reference (read-only). */
    template<class Flag = std::enable_if_t<IsReadOnly>>
    constexpr Any(const IAny &any) noexcept
    {
        set_any(any, TYPE_UID);
    }
    /** @brief Wraps an existing const IAny pointer (read-only). */
    template<class Flag = std::enable_if_t<IsReadOnly>>
    constexpr Any(const IAny *any) noexcept
    {
        if (any) {
            set_any(*any, TYPE_UID);
        }
    }
    /** @brief Move-constructs from an IAny rvalue. */
    constexpr Any(IAny &&any) noexcept
    {
        if (is_compatible(any, TYPE_UID)) {
            any_ = std::move(any);
        }
    }
    /** @brief Default-constructs an IAny of type T via Strata. */
    Any() noexcept { create(); }
    /** @brief Constructs an IAny of type T and initializes it with @p value. */
    Any(const T &value) noexcept
    {
        if (!is_compatible(any_, TYPE_UID)) {
            auto any = instance().create_any(TYPE_UID);
            any_.reset(any.get());
        }
        any_->set_data(&value, TYPE_SIZE, TYPE_UID);
    }
    /** @brief Returns a const reference to the underlying IAny. */
    operator const IAny &() const noexcept { return *(any_.get()); }
    /** @brief Returns the type id of the any value. */
    constexpr Uid get_type_uid() const noexcept { return TYPE_UID; }
    /** @brief Returns a copy of the stored value. */
    T get_value() const noexcept
    {
        std::remove_const_t<T> value{};
        if (any_) {
            any_->get_data(&value, sizeof(T), TYPE_UID);
        }
        return value;
    }
    /** @brief Overwrites the stored value with @p value (read-write only). */
    template<class Flag = std::enable_if_t<IsReadWrite>>
    void set_value(const T &value) noexcept
    {
        if (any_) {
            any_->set_data(&value, sizeof(T), TYPE_UID);
        }
    }

    /** @brief Creates a read-write typed view over an existing IAny pointer. */
    static Any<T> ref(const IAny::Ptr &ref) { return Any<T>(ref); }
    /** @brief Creates a read-only typed view over an existing const IAny pointer. */
    static const Any<const T> const_ref(const IAny::ConstPtr &ref) { return Any<const T>(ref); }

protected:
    void create() { set_any_direct(instance().create_any(TYPE_UID)); }
};

} // namespace strata

#endif // ANY_H
