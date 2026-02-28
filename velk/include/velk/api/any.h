#ifndef VELK_API_ANY_H
#define VELK_API_ANY_H

#include <velk/api/traits.h>
#include <velk/api/velk.h>
#include <velk/array_view.h>
#include <velk/common.h>
#include <velk/ext/any.h>
#include <velk/interface/intf_any.h>
#include <velk/interface/intf_array_any.h>
#include <velk/interface/intf_velk.h>
#include <velk/vector.h>

#include <cassert>
#include <initializer_list>
#include <type_traits>

namespace velk {

namespace detail {

/** @brief Non-template storage for Any<T>. Compiled once regardless of T instantiations. */
class AnyStorage
{
protected:
    AnyStorage() = default;

    void set_any(const IAny& any, const Uid& req) noexcept
    {
        if (is_compatible(any, req)) {
            set_any_direct(any);
        }
    }
    void set_any(const IAny::ConstPtr& any, const Uid& req) noexcept
    {
        if (is_compatible(any, req)) {
            set_any_direct(any);
        }
    }
    void set_any_direct(const IAny& any) noexcept { any_ = refcnt_ptr<IAny>(const_cast<IAny*>(&any)); }
    void set_any_direct(const IAny::ConstPtr& any) noexcept { set_any_direct(*(any.get())); }

public:
    /** @brief Implicit conversion to a const IAny pointer. */
    operator const IAny*() const noexcept { return any_.get(); }
    /** @brief Returns a const reference to the underlying IAny. */
    operator const IAny&() const noexcept { return *(any_.get()); }
    /** @brief Returns true if the wrapper holds a valid IAny. */
    operator bool() const noexcept { return any_.operator bool(); }

protected:
    /** @brief Returns the underlying const IAny pointer. */
    const IAny* get_any_interface() const noexcept { return any_.get(); }

    refcnt_ptr<IAny> any_;
};

/** @brief Non-template storage for ArrayAny<T>. Compiled once regardless of T instantiations. */
class ArrayAnyStorage
{
protected:
    ArrayAnyStorage() = default;

    void set_from_mutable(const IAny::Ptr& any) noexcept { arr_ = interface_pointer_cast<IArrayAny>(any); }
    void set_from_const(const IAny::ConstPtr& any) noexcept
    {
        if (any) {
            auto* nonConst = const_cast<IAny*>(any.get());
            if (auto* aa = interface_cast<IArrayAny>(nonConst)) {
                arr_ = IArrayAny::Ptr(aa, any.block());
            }
        }
    }

    void create_from_buffer(const void* data, size_t count, Uid vecUid, Uid elemUid) noexcept
    {
        set_from_mutable(instance().create_any(vecUid));
        if (arr_) {
            arr_->set_from_buffer(data, count, elemUid);
        }
    }

public:
    /** @brief Returns true if the wrapper holds a valid IArrayAny-backed IAny. */
    operator bool() const noexcept { return arr_.operator bool(); }
    /** @brief Implicit conversion to const IAny pointer. */
    operator const IAny*() const noexcept { return arr_ ? interface_cast<IAny>(arr_) : nullptr; }
    /** @brief Returns the number of elements. */
    size_t size() const { return arr_ ? arr_->array_size() : 0; }
    /** @brief Returns true if the array is empty. */
    bool empty() const { return size() == 0; }

protected:
    IAny* get_any() const { return arr_ ? interface_cast<IAny>(arr_) : nullptr; }

    IArrayAny::Ptr arr_;
};

} // namespace detail

/**
 * @brief Typed wrapper for IAny that provides get_value/set_value accessors for type T.
 *
 * Can be constructed from an existing IAny or will create a new one from Velk.
 * Use const T for read-only access.
 *
 * @tparam T The value type.
 */
template <class T>
class Any final : public detail::AnyStorage
{
    static constexpr bool IsReadWrite = !std::is_const_v<T>;
    static constexpr bool IsReadOnly = !IsReadWrite;
    static constexpr auto TYPE_SIZE = sizeof(T);

public:
    static constexpr Uid TYPE_UID = type_uid<std::remove_const_t<T>>();

    /** @brief Wraps an existing mutable IAny pointer (read-write). */
    template <bool RW = IsReadWrite, detail::require<RW> = 0>
    constexpr Any(const IAny::Ptr& any) noexcept
    {
        set_any(any, TYPE_UID);
    }
    /** @brief Wraps an existing const IAny pointer (read-only). */
    template <bool RO = IsReadOnly, detail::require<RO> = 0>
    constexpr Any(const IAny::ConstPtr& any) noexcept
    {
        set_any(any, TYPE_UID);
    }
    /** @brief Wraps a const IAny reference (read-only). */
    template <bool RO = IsReadOnly, detail::require<RO> = 0>
    constexpr Any(const IAny& any) noexcept
    {
        set_any(any, TYPE_UID);
    }
    /** @brief Wraps an existing const IAny pointer (read-only). */
    template <bool RO = IsReadOnly, detail::require<RO> = 0>
    constexpr Any(const IAny* any) noexcept
    {
        if (any) {
            set_any(*any, TYPE_UID);
        }
    }
    /** @brief Move-constructs from an IAny rvalue. */
    constexpr Any(IAny&& any) noexcept
    {
        if (is_compatible(any, TYPE_UID)) {
            any_ = std::move(any);
        }
    }
    /** @brief Default-constructs an IAny of type T via Velk. */
    Any() noexcept { set_any_direct(instance().create_any(TYPE_UID)); }
    /** @brief Constructs an IAny of type T and initializes it with @p value. */
    Any(const T& value) noexcept
    {
        if (!is_compatible(any_, TYPE_UID)) {
            auto any = instance().create_any(TYPE_UID);
            any_.reset(any.get());
        }
        any_->set_data(&value, TYPE_SIZE, TYPE_UID);
    }

    /** @brief Implicit conversion to a mutable IAny pointer (read-write only). */
    template <bool RW = IsReadWrite, detail::require<RW> = 0>
    operator IAny*() noexcept
    {
        return any_.get();
    }
    /** @brief Returns the underlying mutable IAny pointer (read-write only). */
    template <bool RW = IsReadWrite, detail::require<RW> = 0>
    IAny* get_any_interface() noexcept
    {
        return any_.get();
    }

    /** @brief Copies the value from @p other into the managed IAny (read-write only). */
    template <bool RW = IsReadWrite, detail::require<RW> = 0>
    bool copy_from(const IAny& other)
    {
        return any_ && any_->copy_from(other);
    }

    /** @brief Returns the type id of the any value. */
    constexpr Uid get_type_uid() const noexcept { return TYPE_UID; }

    /** @brief Return a clone of the any value */
    IAny::Ptr clone() const { return any_ ? any_->clone() : nullptr; }

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
    template <bool RW = IsReadWrite, detail::require<RW> = 0>
    void set_value(const T& value) noexcept
    {
        if (any_) {
            any_->set_data(&value, sizeof(T), TYPE_UID);
        }
    }

    /** @brief Creates a read-write typed view over an existing IAny pointer. */
    static Any<T> ref(const IAny::Ptr& ref) { return Any<T>(ref); }
    /** @brief Creates a read-only typed view over an existing const IAny pointer. */
    static const Any<const T> const_ref(const IAny::ConstPtr& ref) { return Any<const T>(ref); }
};

/**
 * @brief Typed wrapper for an IAny that also implements IArrayAny.
 *
 * Provides typed element access (at, set_at, push_back, erase_at, clear)
 * without requiring the caller to create temporary AnyValue objects.
 * Use const T for read-only access.
 *
 * @tparam T The element type.
 */
template <class T>
class ArrayAny : public detail::ArrayAnyStorage
{
    using Type = std::remove_const_t<T>;
    static constexpr bool IsReadWrite = !std::is_const_v<T>;
    static constexpr bool IsReadOnly = !IsReadWrite;

public:
    static constexpr Uid ELEM_UID = type_uid<Type>();
    static constexpr Uid VEC_UID = type_uid<vector<Type>>();

    /** @brief Wraps an existing mutable IAny pointer that implements IArrayAny (read-write). */
    template <bool RW = IsReadWrite, detail::require<RW> = 0>
    explicit ArrayAny(const IAny::Ptr& any) noexcept
    {
        set_from_mutable(any);
    }

    /** @brief Wraps an existing const IAny pointer that implements IArrayAny (read-only). */
    template <bool RO = IsReadOnly, detail::require<RO> = 0>
    explicit ArrayAny(const IAny::ConstPtr& any) noexcept
    {
        set_from_const(any);
    }

    /** @brief Constructs an owned empty array (read-write only). */
    template <bool RW = IsReadWrite, detail::require<RW> = 0>
    ArrayAny() noexcept
    {
        set_from_mutable(instance().create_any(VEC_UID));
    }

    /** @brief Constructs an owned array from an initializer list (read-write only). */
    template <bool RW = IsReadWrite, detail::require<RW> = 0>
    ArrayAny(std::initializer_list<Type> init) noexcept
    {
        create_from_buffer(init.begin(), init.size(), VEC_UID, ELEM_UID);
    }

    /** @brief Constructs an owned array from an array_view (read-write only). */
    template <bool RW = IsReadWrite, detail::require<RW> = 0>
    ArrayAny(array_view<Type> view) noexcept
    {
        create_from_buffer(view.begin(), view.size(), VEC_UID, ELEM_UID);
    }

    /** @brief Returns the element at @p index, or default-constructed T on failure. */
    Type at(size_t index) const
    {
        Type value{};
        if (arr_) {
            ext::AnyValue<Type> out;
            if (succeeded(arr_->get_at(index, out))) {
                value = out.get_value();
            }
        }
        return value;
    }

    /** @brief Returns a full copy of the vector. */
    vector<Type> get_value() const
    {
        vector<Type> result;
        if (auto* any = get_any()) {
            any->get_data(&result, sizeof(vector<Type>), VEC_UID);
        }
        return result;
    }

    /** @brief Sets the element at @p index to @p value (read-write only). */
    template <bool RW = IsReadWrite, detail::require<RW> = 0>
    ReturnValue set_at(size_t index, const Type& value)
    {
        if (!arr_) {
            return ReturnValue::Fail;
        }
        ext::AnyValue<Type> av;
        av.set_value(value);
        return arr_->set_at(index, av);
    }

    /** @brief Appends @p value to the end (read-write only). */
    template <bool RW = IsReadWrite, detail::require<RW> = 0>
    ReturnValue push_back(const Type& value)
    {
        if (!arr_) {
            return ReturnValue::Fail;
        }
        ext::AnyValue<Type> av;
        av.set_value(value);
        return arr_->push_back(av);
    }

    /** @brief Erases the element at @p index (read-write only). */
    template <bool RW = IsReadWrite, detail::require<RW> = 0>
    ReturnValue erase_at(size_t index)
    {
        return arr_ ? arr_->erase_at(index) : ReturnValue::Fail;
    }

    /** @brief Removes all elements (read-write only). */
    template <bool RW = IsReadWrite, detail::require<RW> = 0>
    void clear()
    {
        if (arr_) {
            arr_->clear_array();
        }
    }

    /** @brief Sets the whole vector value (read-write only). */
    template <bool RW = IsReadWrite, detail::require<RW> = 0>
    ReturnValue set_value(const vector<Type>& value)
    {
        auto* any = get_any();
        return any ? any->set_data(&value, sizeof(vector<Type>), VEC_UID) : ReturnValue::Fail;
    }
};

} // namespace velk

#endif // ANY_H
