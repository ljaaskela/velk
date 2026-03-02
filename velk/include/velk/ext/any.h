#ifndef VELK_EXT_ANY_H
#define VELK_EXT_ANY_H

#include <velk/common.h>
#include <velk/ext/core_object.h>
#include <velk/interface/intf_any.h>
#include <velk/interface/intf_array_any.h>
#include <velk/interface/intf_velk.h>
#include <velk/vector.h>

#include <cstring>
#include <type_traits>

namespace velk::detail {

template <class T>
constexpr bool has_iany_in_chain()
{
    if constexpr (std::is_same_v<T, IAny>) {
        return true;
    } else if constexpr (std::is_same_v<T, IInterface>) {
        return false;
    } else {
        return has_iany_in_chain<typename T::ParentInterface>();
    }
}

/**
 * @brief Non-template base providing IAny method implementations for trivially copyable types.
 *
 * Derived classes pass raw storage pointers and runtime type info (elem_size, type_uid).
 * All methods are compiled once and shared across all trivial AnyCore instantiations.
 */
class any_core_base
{
public:
    static ReturnValue get_data(const void* storage, void* to, size_t elem_size)
    {
        std::memcpy(to, storage, elem_size);
        return ReturnValue::Success;
    }

    static ReturnValue set_value(void* storage, const void* new_val, size_t elem_size)
    {
        if (std::memcmp(storage, new_val, elem_size) != 0) {
            std::memcpy(storage, new_val, elem_size);
            return ReturnValue::Success;
        }
        return ReturnValue::NothingToDo;
    }

    static ReturnValue copy_from(void* storage, const IAny& other, size_t elem_size, Uid type_uid)
    {
        if (!is_compatible(other, type_uid)) {
            return ReturnValue::Fail;
        }
        alignas(8) char buf[sizeof(double)];
        if (elem_size <= sizeof(buf)) {
            return succeeded(other.get_data(buf, elem_size, type_uid)) ? set_value(storage, buf, elem_size)
                                                                       : ReturnValue::Fail;
        }
        void* heap_buf = std::malloc(elem_size);
        ReturnValue ret = ReturnValue::Fail;
        if (heap_buf && succeeded(other.get_data(heap_buf, elem_size, type_uid))) {
            ret = set_value(storage, heap_buf, elem_size);
        }
        std::free(heap_buf);
        return ret;
    }
};

/**
 * @brief Non-template base providing IArrayAny method implementations via type-erased operations.
 *
 * All methods are compiled once and shared across all trivial ArrayAnyCore instantiations
 * via ICF (identical COMDAT folding).
 */
class array_any_core_base
{
public:
    static ReturnValue get_at(const vector_base& vec, size_t index, IAny& out, size_t elem_size, Uid elem_uid)
    {
        if (index >= vec.size_) {
            return ReturnValue::InvalidArgument;
        }
        return out.set_data(static_cast<const char*>(vec.data_) + index * elem_size, elem_size, elem_uid);
    }

    static ReturnValue set_at(vector_base& vec, size_t index, const IAny& value, size_t elem_size,
                              Uid elem_uid)
    {
        if (index >= vec.size_) {
            return ReturnValue::InvalidArgument;
        }
        return value.get_data(static_cast<char*>(vec.data_) + index * elem_size, elem_size, elem_uid);
    }

    static ReturnValue push_back(vector_base& vec, const IAny& value, size_t elem_size, Uid elem_uid)
    {
        size_t old_size = vec.size_;
        resize(vec, old_size + 1, elem_size);
        auto rv = value.get_data(static_cast<char*>(vec.data_) + old_size * elem_size, elem_size, elem_uid);
        if (failed(rv)) {
            vec.size_ = old_size;
            return ReturnValue::InvalidArgument;
        }
        return ReturnValue::Success;
    }

    static ReturnValue set_from_buffer(vector_base& vec, const void* data, size_t count, Uid elementType,
                                       size_t elem_size, Uid elem_uid)
    {
        if (elementType != elem_uid) {
            return ReturnValue::InvalidArgument;
        }
        resize(vec, count, elem_size);
        std::memcpy(vec.data_, data, count * elem_size);
        return ReturnValue::Success;
    }

private:
    static void resize(vector_base& vec, size_t count, size_t elem_size)
    {
        if (count > vec.capacity_) {
            vec.grow_raw(count, elem_size);
        }
        if (count > vec.size_) {
            std::memset(
                static_cast<char*>(vec.data_) + vec.size_ * elem_size, 0, (count - vec.size_) * elem_size);
        }
        vec.size_ = count;
    }
};

} // namespace velk::detail

namespace velk::ext {

template <bool HasIAny, class FinalClass, class... Interfaces>
struct AnyBaseParent;

template <class FinalClass, class... Interfaces>
struct AnyBaseParent<true, FinalClass, Interfaces...>
{
    using type = ObjectCore<FinalClass, Interfaces...>;
};

template <class FinalClass, class... Interfaces>
struct AnyBaseParent<false, FinalClass, Interfaces...>
{
    using type = ObjectCore<FinalClass, IAny, Interfaces...>;
};

/**
 * @brief Base class for IAny implementations.
 *
 * Conditionally prepends IAny to the interface pack. If any interface in the pack
 * already has IAny in its parent chain (e.g. IArrayAny), IAny is not added redundantly.
 *
 * @tparam FinalClass The final derived class (CRTP parameter).
 * @tparam Interfaces Additional interfaces beyond IAny.
 */
template <class FinalClass, class... Interfaces>
class AnyBase : public AnyBaseParent<(::velk::detail::has_iany_in_chain<Interfaces>() || ...), FinalClass,
                                     Interfaces...>::type
{
public:
    /** @brief Creates a clone by instantiating a new FinalClass and copying data into it. */
    IAny::Ptr clone() const override
    {
        auto clone = FinalClass::get_factory().template create_instance<IAny>();
        return clone && succeeded(clone->copy_from(*this)) ? clone : nullptr;
    }
};

/**
 * @brief AnyBase specialization that declares compatibility with one or more types.
 * @tparam FinalClass The final derived class (CRTP parameter).
 * @tparam Types The data types this any is compatible with.
 */
template <class FinalClass, class... Types>
class AnyMulti : public AnyBase<FinalClass>
{
public:
    static constexpr Uid TYPE_UID = type_uid<Types...>();

    /** @brief Returns the list of type UIDs this any is compatible with. */
    array_view<Uid> get_compatible_types() const override
    {
        static constexpr Uid uids[] = {(type_uid<Types>())...};
        return {uids, sizeof...(Types)};
    }

    /** @brief Returns the UID for the combined type pack. */
    static constexpr Uid class_id() { return TYPE_UID; }
};

/**
 * @brief A helper template for implementing an Any which supports a single type
 */
template <class FinalClass, class T, class... Interfaces>
class AnyCore : public AnyBase<FinalClass, Interfaces...>
{
public:
    static constexpr Uid TYPE_UID = type_uid<T>();
    /** @brief Returns a const reference to the stored value. */
    virtual const T& get_value() const = 0;

    /** @brief Sets the stored value. Returns Success if changed, NothingToDo if identical. */
    virtual ReturnValue set_value(const T& value)
    {
        if constexpr (std::is_trivially_copyable_v<T>) {
            return ::velk::detail::any_core_base::set_value(const_cast<T*>(&get_value()), &value, sizeof(T));
        } else {
            auto& ref = const_cast<T&>(get_value());
            if (ref != value) {
                ref = value;
                return ReturnValue::Success;
            }
            return ReturnValue::NothingToDo;
        }
    }

    /** @brief Returns the UID for type T. */
    static constexpr Uid class_id() { return TYPE_UID; }

    /** @brief Returns a single-element list containing TYPE_UID. */
    array_view<Uid> get_compatible_types() const override
    {
        static constexpr Uid uid = TYPE_UID;
        return {&uid, 1};
    }
    size_t get_data_size(Uid type) const override { return type == TYPE_UID ? sizeof(T) : 0; }

    ReturnValue get_data(void* to, size_t toSize, Uid type) const override
    {
        if (!is_valid_args(to, toSize, type)) {
            return ReturnValue::Fail;
        }
        if constexpr (std::is_trivially_copyable_v<T>) {
            return ::velk::detail::any_core_base::get_data(&get_value(), to, toSize);
        } else {
            *reinterpret_cast<T*>(to) = get_value();
            return ReturnValue::Success;
        }
    }

    ReturnValue set_data(void const* from, size_t fromSize, Uid type) override
    {
        if (!is_valid_args(from, fromSize, type)) {
            return ReturnValue::Fail;
        }
        return set_value(*reinterpret_cast<const T*>(from));
    }

    ReturnValue copy_from(const IAny& other) override
    {
        if constexpr (std::is_trivially_copyable_v<T>) {
            if constexpr (sizeof(T) <= sizeof(double)) {
                // Small trivial: use the non-template base with stack buffer
                alignas(T) char buf[sizeof(T)];
                if (!is_compatible(other, TYPE_UID)) {
                    return ReturnValue::Fail;
                }
                return succeeded(other.get_data(buf, sizeof(T), TYPE_UID))
                           ? set_value(*reinterpret_cast<const T*>(buf))
                           : ReturnValue::Fail;
            } else {
                // Large trivial (e.g. vector<T>): delegate to base
                return ::velk::detail::any_core_base::copy_from(
                    const_cast<T*>(&get_value()), other, sizeof(T), TYPE_UID);
            }
        } else {
            if (!is_compatible(other, TYPE_UID)) {
                return ReturnValue::Fail;
            }
            T value{};
            return succeeded(other.get_data(&value, sizeof(T), TYPE_UID)) ? set_value(value)
                                                                          : ReturnValue::Fail;
        }
    }

private:
    static bool is_valid_args(const void* from, size_t fromSize, Uid type) noexcept
    {
        return from && type == TYPE_UID && fromSize == sizeof(T);
    }
};

/**
 * @brief A basic Any implementation with a single supported data type which is stored in local storage.
 */
template <class T>
class AnyValue final : public AnyCore<AnyValue<T>, T>
{
public:
    const T& get_value() const override { return data_; }

private:
    T data_{};
};

/**
 * @brief An Any implementation that reads/writes through a pointer to external storage.
 *
 * Does not own the data. Useful for ECS-style patterns where all property values live
 * in a contiguous struct that can be memcpy'd. Cloning produces an owned AnyValue<T> copy.
 */
template <class T>
class AnyRef final : public AnyCore<AnyRef<T>, T>
{
    using Base = AnyCore<AnyRef<T>, T>;

public:
    explicit AnyRef(T* ptr = nullptr) : ptr_(ptr) {}

    /** @brief Retargets this any to a different memory location. */
    void set_target(T* ptr) { ptr_ = ptr; }

    const T& get_value() const override { return *ptr_; }

    /** @brief Clones as an owned AnyValue<T> (snapshot of the referenced data). */
    IAny::Ptr clone() const override
    {
        auto c = AnyValue<T>::get_factory().template create_instance<IAny>();
        return c && succeeded(c->copy_from(*this)) ? c : nullptr;
    }

private:
    T* ptr_{};
};

/**
 * @brief CRTP base for ArrayAnyRef/ArrayAnyValue, implementing all IAny and IArrayAny methods.
 *
 * Derived classes provide vec() returning vector<T>& and constructors. This eliminates
 * the code duplication between ref and value variants.
 *
 * Bypasses AnyCore to avoid the virtual get_value/set_value indirection. IArrayAny methods
 * delegate to non-template helpers via static constexpr elem_size_/elem_uid_.
 *
 * @tparam Derived The CRTP derived class (ArrayAnyRef<T> or ArrayAnyValue<T>).
 * @tparam T The element type of the vector.
 */
template <class Derived, class T>
class ArrayAnyCore : public AnyBase<Derived, IArrayAny>
{
protected:
    using vec_type = vector<T>;
    static constexpr Uid VEC_UID = type_uid<vec_type>();
    static constexpr size_t elem_size_ = sizeof(T);
    static constexpr Uid elem_uid_ = type_uid<T>();

    vec_type& vec() { return static_cast<Derived*>(this)->vec(); }
    const vec_type& vec() const { return static_cast<const Derived*>(this)->vec(); }

public:
    static constexpr Uid class_id() { return VEC_UID; }

    // IAny
    array_view<Uid> get_compatible_types() const override
    {
        static constexpr Uid uid = VEC_UID;
        return {&uid, 1};
    }

    size_t get_data_size(Uid type) const override { return type == VEC_UID ? sizeof(vec_type) : 0; }

    ReturnValue get_data(void* to, size_t toSize, Uid type) const override
    {
        if (!is_valid_args(to, toSize, type)) {
            return ReturnValue::Fail;
        }
        *static_cast<vec_type*>(to) = vec();
        return ReturnValue::Success;
    }

    ReturnValue set_data(const void* from, size_t fromSize, Uid type) override
    {
        if (!is_valid_args(from, fromSize, type)) {
            return ReturnValue::Fail;
        }
        const auto& v = *static_cast<const vec_type*>(from);
        auto& vec = this->vec();
        if (vec != v) {
            vec = v;
            return ReturnValue::Success;
        }
        return ReturnValue::NothingToDo;
    }

    ReturnValue copy_from(const IAny& other) override
    {
        if (!is_compatible(other, VEC_UID)) {
            return ReturnValue::Fail;
        }
        vec_type tmp;
        if (failed(other.get_data(&tmp, sizeof(vec_type), VEC_UID))) {
            return ReturnValue::Fail;
        }
        auto& vec = this->vec();
        if (vec != tmp) {
            vec = static_cast<vec_type&&>(tmp);
            return ReturnValue::Success;
        }
        return ReturnValue::NothingToDo;
    }

    // IArrayAny
    size_t array_size() const override { return vec().size(); }

    ReturnValue get_at(size_t index, IAny& out) const override
    {
        if constexpr (std::is_trivially_copyable_v<T>) {
            return ::velk::detail::array_any_core_base::get_at(
                vec().base(), index, out, elem_size_, elem_uid_);
        } else {
            auto& vec = this->vec();
            if (index >= vec.size()) {
                return ReturnValue::InvalidArgument;
            }
            return out.set_data(&vec[index], sizeof(T), elem_uid_);
        }
    }

    ReturnValue set_at(size_t index, const IAny& value) override
    {
        if constexpr (std::is_trivially_copyable_v<T>) {
            return ::velk::detail::array_any_core_base::set_at(
                vec().base(), index, value, elem_size_, elem_uid_);
        } else {
            auto& vec = this->vec();
            if (index >= vec.size()) {
                return ReturnValue::InvalidArgument;
            }
            return value.get_data(&vec[index], sizeof(T), elem_uid_);
        }
    }

    ReturnValue push_back(const IAny& value) override
    {
        if constexpr (std::is_trivially_copyable_v<T>) {
            return ::velk::detail::array_any_core_base::push_back(vec().base(), value, elem_size_, elem_uid_);
        } else {
            T elem{};
            if (failed(value.get_data(&elem, sizeof(T), elem_uid_))) {
                return ReturnValue::InvalidArgument;
            }
            vec().push_back(elem);
            return ReturnValue::Success;
        }
    }

    ReturnValue erase_at(size_t index) override
    {
        auto& vec = this->vec();
        if (index >= vec.size()) {
            return ReturnValue::InvalidArgument;
        }
        vec.erase(vec.begin() + index);
        return ReturnValue::Success;
    }

    void clear_array() override { vec().clear(); }

    ReturnValue set_from_buffer(const void* data, size_t count, Uid elementType) override
    {
        if constexpr (std::is_trivially_copyable_v<T>) {
            return ::velk::detail::array_any_core_base::set_from_buffer(
                vec().base(), data, count, elementType, elem_size_, elem_uid_);
        } else {
            if (elementType != elem_uid_) {
                return ReturnValue::InvalidArgument;
            }
            auto* elems = reinterpret_cast<const T*>(data);
            vec() = vec_type(elems, elems + count);
            return ReturnValue::Success;
        }
    }

private:
    static bool is_valid_args(const void* from, size_t fromSize, Uid type) noexcept
    {
        return from && type == VEC_UID && fromSize == sizeof(vec_type);
    }
};

/**
 * @brief An AnyRef for vector<T> that also implements IArrayAny for element-level access.
 *
 * Points to external vector<T> storage (e.g. in a State struct).
 * Clones as an owned AnyValue<vector<T>>.
 *
 * @tparam T The element type of the vector.
 */
template <class T>
class ArrayAnyRef final : public ArrayAnyCore<ArrayAnyRef<T>, T>
{
    using vec_type = vector<T>;
    friend class ArrayAnyCore<ArrayAnyRef<T>, T>;

public:
    explicit ArrayAnyRef(vec_type* ptr = nullptr) : ptr_(ptr) {}

    IAny::Ptr clone() const override
    {
        auto c = AnyValue<vec_type>::get_factory().template create_instance<IAny>();
        return c && succeeded(c->copy_from(*this)) ? c : nullptr;
    }

private:
    vec_type& vec() { return *ptr_; }
    const vec_type& vec() const { return *ptr_; }

    vec_type* ptr_{};
};

/**
 * @brief An owned Any for vector<T> that implements IArrayAny for element-level access.
 *
 * Stores the vector inline (like AnyValue<T>).
 *
 * @tparam T The element type of the vector.
 */
template <class T>
class ArrayAnyValue final : public ArrayAnyCore<ArrayAnyValue<T>, T>
{
    using vec_type = vector<T>;
    friend class ArrayAnyCore<ArrayAnyValue<T>, T>;

public:
    ArrayAnyValue() = default;
    explicit ArrayAnyValue(const vec_type& value) : data_(value) {}

private:
    vec_type& vec() { return data_; }
    const vec_type& vec() const { return data_; }

    vec_type data_{};
};

/** @brief Creates an ArrayAnyRef<T> wrapped in a shared_ptr, pointing to the given vector. */
template <class T>
IAny::Ptr create_array_any_ref(vector<T>* ptr)
{
    auto* obj = new ArrayAnyRef<T>(ptr);
    return IAny::Ptr(static_cast<IAny*>(obj));
}

/** @brief Creates an AnyRef<T> wrapped in a shared_ptr, pointing to the given address. */
template <class T>
IAny::Ptr create_any_ref(T* ptr)
{
    auto* obj = new AnyRef<T>(ptr);
    return IAny::Ptr(static_cast<IAny*>(obj));
}

} // namespace velk::ext

#endif // ANY_H
