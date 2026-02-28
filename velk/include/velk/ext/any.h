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
class AnyBase : public AnyBaseParent<
    (::velk::detail::has_iany_in_chain<Interfaces>() || ...),
    FinalClass, Interfaces...>::type
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
    /** @brief Sets the stored value. Returns Success if changed, NothingToDo if identical. */
    virtual ReturnValue set_value(const T& value) = 0;
    /** @brief Returns a const reference to the stored value. */
    virtual const T& get_value() const = 0;

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
        if (is_valid_args(to, toSize, type)) {
            *reinterpret_cast<T*>(to) = get_value();
            return ReturnValue::Success;
        }
        return ReturnValue::Fail;
    }
    ReturnValue set_data(void const* from, size_t fromSize, Uid type) override
    {
        if (is_valid_args(from, fromSize, type)) {
            return set_value(*reinterpret_cast<const T*>(from));
        }
        return ReturnValue::Fail;
    }
    ReturnValue copy_from(const IAny& other) override
    {
        if (is_compatible(other, TYPE_UID)) {
            T value;
            if (succeeded(other.get_data(&value, sizeof(T), TYPE_UID))) {
                return set_value(value);
            }
        }
        return ReturnValue::Fail;
    }

private:
    static constexpr bool is_valid_args(void const* to, size_t toSize, Uid type)
    {
        return to && type == TYPE_UID && sizeof(T) == toSize;
    }
};

/**
 * @brief A basic Any implementation with a single supported data type which is stored in local storage.
 *
 * Overrides data operations to access storage directly, avoiding virtual get_value()/set_value() dispatch.
 * For trivially copyable types, uses memcpy/memcmp instead of typed operations.
 */
template <class T>
class AnyValue final : public AnyCore<AnyValue<T>, T>
{
    using Base = AnyCore<AnyValue<T>, T>;

public:
    ReturnValue set_value(const T& value) override
    {
        if constexpr (std::is_trivially_copyable_v<T>) {
            if (std::memcmp(&data_, &value, sizeof(T)) != 0) {
                std::memcpy(&data_, &value, sizeof(T));
                return ReturnValue::Success;
            }
        } else {
            if (data_ != value) {
                data_ = value;
                return ReturnValue::Success;
            }
        }
        return ReturnValue::NothingToDo;
    }

    const T& get_value() const override { return data_; }

    size_t get_data_size(Uid type) const override { return type == Base::TYPE_UID ? sizeof(T) : 0; }

    ReturnValue get_data(void* to, size_t toSize, Uid type) const override
    {
        if (!valid_args(to, toSize, type)) {
            return ReturnValue::Fail;
        }
        if constexpr (std::is_trivially_copyable_v<T>) {
            std::memcpy(to, &data_, sizeof(T));
        } else {
            *reinterpret_cast<T*>(to) = data_;
        }
        return ReturnValue::Success;
    }

    ReturnValue set_data(const void* from, size_t fromSize, Uid type) override
    {
        return valid_args(from, fromSize, type) ? set_value(*reinterpret_cast<const T*>(from))
                                                : ReturnValue::Fail;
    }

    ReturnValue copy_from(const IAny& other) override
    {
        if (!is_compatible(other, Base::TYPE_UID)) {
            return ReturnValue::Fail;
        }
        if constexpr (std::is_trivially_copyable_v<T>) {
            char buf[sizeof(T)];
            return succeeded(other.get_data(buf, sizeof(T), Base::TYPE_UID))
                       ? set_value(*reinterpret_cast<const T*>(buf))
                       : ReturnValue::Fail;
        } else {
            T value{};
            return succeeded(other.get_data(&value, sizeof(T), Base::TYPE_UID)) ? set_value(value)
                                                                                : ReturnValue::Fail;
        }
    }

private:
    static constexpr bool valid_args(const void* p, size_t size, Uid type)
    {
        return p && type == Base::TYPE_UID && size == sizeof(T);
    }
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

    ReturnValue set_value(const T& value) override
    {
        if constexpr (std::is_trivially_copyable_v<T>) {
            if (std::memcmp(ptr_, &value, sizeof(T)) != 0) {
                std::memcpy(ptr_, &value, sizeof(T));
                return ReturnValue::Success;
            }
        } else {
            if (*ptr_ != value) {
                *ptr_ = value;
                return ReturnValue::Success;
            }
        }
        return ReturnValue::NothingToDo;
    }

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
 * @brief An AnyRef for vector<T> that also implements IArrayAny for element-level access.
 *
 * Points to external vector<T> storage (e.g. in a State struct). Provides both
 * whole-vector IAny operations (get/set the entire vector) and element-level
 * operations (get_at, set_at, push_back, erase_at, clear_array) via IArrayAny.
 *
 * @tparam T The element type of the vector.
 */
template <class T>
class ArrayAnyRef final : public AnyCore<ArrayAnyRef<T>, vector<T>, IArrayAny>
{
    using Base = AnyCore<ArrayAnyRef<T>, vector<T>, IArrayAny>;
    using vec_type = vector<T>;

public:
    explicit ArrayAnyRef(vec_type* ptr = nullptr) : ptr_(ptr) {}

    ReturnValue set_value(const vec_type& value) override
    {
        if (*ptr_ != value) {
            *ptr_ = value;
            return ReturnValue::Success;
        }
        return ReturnValue::NothingToDo;
    }

    const vec_type& get_value() const override { return *ptr_; }

    IAny::Ptr clone() const override
    {
        auto c = AnyValue<vec_type>::get_factory().template create_instance<IAny>();
        return c && succeeded(c->copy_from(*this)) ? c : nullptr;
    }

    // IArrayAny
    size_t array_size() const override { return ptr_->size(); }

    ReturnValue get_at(size_t index, IAny& out) const override
    {
        if (index >= ptr_->size()) {
            return ReturnValue::InvalidArgument;
        }
        return out.set_data(&(*ptr_)[index], sizeof(T), type_uid<T>());
    }

    ReturnValue set_at(size_t index, const IAny& value) override
    {
        if (index >= ptr_->size()) {
            return ReturnValue::InvalidArgument;
        }
        T elem{};
        auto ret = value.get_data(&elem, sizeof(T), type_uid<T>());
        if (failed(ret)) {
            return ReturnValue::InvalidArgument;
        }
        (*ptr_)[index] = elem;
        return ReturnValue::Success;
    }

    ReturnValue push_back(const IAny& value) override
    {
        T elem{};
        auto ret = value.get_data(&elem, sizeof(T), type_uid<T>());
        if (failed(ret)) {
            return ReturnValue::InvalidArgument;
        }
        ptr_->push_back(elem);
        return ReturnValue::Success;
    }

    ReturnValue erase_at(size_t index) override
    {
        if (index >= ptr_->size()) {
            return ReturnValue::InvalidArgument;
        }
        ptr_->erase(ptr_->begin() + index);
        return ReturnValue::Success;
    }

    void clear_array() override { ptr_->clear(); }

    ReturnValue set_from_buffer(const void* data, size_t count, Uid elementType) override
    {
        if (elementType != type_uid<T>()) {
            return ReturnValue::InvalidArgument;
        }
        auto* elems = reinterpret_cast<const T*>(data);
        *ptr_ = vec_type(elems, elems + count);
        return ReturnValue::Success;
    }

private:
    vec_type* ptr_{};
};

/**
 * @brief An owned Any for vector<T> that implements IArrayAny for element-level access.
 *
 * Stores the vector inline (like AnyValue<T>). Provides both whole-vector IAny
 * operations and element-level operations via IArrayAny.
 *
 * @tparam T The element type of the vector.
 */
template <class T>
class ArrayAnyValue final : public AnyCore<ArrayAnyValue<T>, vector<T>, IArrayAny>
{
    using Base = AnyCore<ArrayAnyValue<T>, vector<T>, IArrayAny>;
    using vec_type = vector<T>;

public:
    ArrayAnyValue() = default;
    explicit ArrayAnyValue(const vec_type& value) : data_(value) {}

    ReturnValue set_value(const vec_type& value) override
    {
        if (data_ != value) {
            data_ = value;
            return ReturnValue::Success;
        }
        return ReturnValue::NothingToDo;
    }

    const vec_type& get_value() const override { return data_; }

    // IArrayAny
    size_t array_size() const override { return data_.size(); }

    ReturnValue get_at(size_t index, IAny& out) const override
    {
        if (index >= data_.size()) {
            return ReturnValue::InvalidArgument;
        }
        return out.set_data(&data_[index], sizeof(T), type_uid<T>());
    }

    ReturnValue set_at(size_t index, const IAny& value) override
    {
        if (index >= data_.size()) {
            return ReturnValue::InvalidArgument;
        }
        T elem{};
        auto ret = value.get_data(&elem, sizeof(T), type_uid<T>());
        if (failed(ret)) {
            return ReturnValue::InvalidArgument;
        }
        data_[index] = elem;
        return ReturnValue::Success;
    }

    ReturnValue push_back(const IAny& value) override
    {
        T elem{};
        auto ret = value.get_data(&elem, sizeof(T), type_uid<T>());
        if (failed(ret)) {
            return ReturnValue::InvalidArgument;
        }
        data_.push_back(elem);
        return ReturnValue::Success;
    }

    ReturnValue erase_at(size_t index) override
    {
        if (index >= data_.size()) {
            return ReturnValue::InvalidArgument;
        }
        data_.erase(data_.begin() + index);
        return ReturnValue::Success;
    }

    void clear_array() override { data_.clear(); }

    ReturnValue set_from_buffer(const void* data, size_t count, Uid elementType) override
    {
        if (elementType != type_uid<T>()) {
            return ReturnValue::InvalidArgument;
        }
        auto* elems = reinterpret_cast<const T*>(data);
        data_ = vec_type(elems, elems + count);
        return ReturnValue::Success;
    }

private:
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
