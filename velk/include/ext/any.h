#ifndef EXT_ANY_H
#define EXT_ANY_H

#include <common.h>
#include <ext/core_object.h>
#include <interface/intf_any.h>
#include <interface/intf_velk.h>

#include <cstring>
#include <type_traits>

namespace velk::ext {

/**
 * @brief Base class for IAny implementations.
 *
 * Inherits RefCountedDispatch directly with IAny (no ObjectCore or metadata).
 *
 * @tparam FinalClass The final derived class (CRTP parameter).
 * @tparam Interfaces Additional interfaces beyond IAny.
 */
template<class FinalClass, class... Interfaces>
class AnyBase : public RefCountedDispatch<IAny, Interfaces...>
{
public:
    /** @brief Returns the compile-time class name of FinalClass. */
    static constexpr string_view get_class_name() { return get_name<FinalClass>(); }
    /** @brief Returns a default UID (overridden by typed subclasses). */
    static constexpr Uid get_class_uid() { return {}; }

    /** @brief Returns a shared_ptr to this object, or empty if expired. */
    IObject::Ptr get_self() const override
    {
        auto* block = this->get_block();
        if (!block || !block->ptr || block->strong.load(std::memory_order_acquire) == 0)
            return {};
        return IObject::Ptr(static_cast<IObject*>(block->ptr), block);
    }

    /** @brief Creates a clone by instantiating a new FinalClass and copying data into it. */
    IAny::Ptr clone() const override
    {
        auto clone = FinalClass::get_factory().template create_instance<IAny>();
        return clone && succeeded(clone->copy_from(*this)) ? clone : nullptr;
    }

    /** @brief Returns the singleton factory for creating instances of FinalClass. */
    static const IObjectFactory &get_factory()
    {
        static DefaultFactory<FinalClass> factory_;
        return factory_;
    }
};

/**
 * @brief AnyBase specialization that declares compatibility with one or more types.
 * @tparam FinalClass The final derived class (CRTP parameter).
 * @tparam Types The data types this any is compatible with.
 */
template<class FinalClass, class... Types>
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
    static constexpr Uid get_class_uid() { return TYPE_UID; }
};

/**
 * @brief A helper template for implementing an Any which supports a single type
 */
template<class FinalClass, class T, class... Interfaces>
class AnyCore : public AnyBase<FinalClass, Interfaces...>
{
public:
    static constexpr Uid TYPE_UID = type_uid<T>();
    /** @brief Sets the stored value. Returns SUCCESS if changed, NOTHING_TO_DO if identical. */
    virtual ReturnValue set_value(const T &value) = 0;
    /** @brief Returns a const reference to the stored value. */
    virtual const T &get_value() const = 0;

    /** @brief Returns the UID for type T. */
    static constexpr Uid get_class_uid() { return TYPE_UID; }

    /** @brief Returns a single-element list containing TYPE_UID. */
    array_view<Uid> get_compatible_types() const override
    {
        static constexpr Uid uid = TYPE_UID;
        return {&uid, 1};
    }
    size_t get_data_size(Uid type) const override
    {
        return type == TYPE_UID ? sizeof(T) : 0;
    }
    ReturnValue get_data(void *to, size_t toSize, Uid type) const override
    {
        if (is_valid_args(to, toSize, type)) {
            *reinterpret_cast<T *>(to) = get_value();
            return ReturnValue::SUCCESS;
        }
        return ReturnValue::FAIL;
    }
    ReturnValue set_data(void const *from, size_t fromSize, Uid type) override
    {
        if (is_valid_args(from, fromSize, type)) {
            return set_value(*reinterpret_cast<const T *>(from));
        }
        return ReturnValue::FAIL;
    }
    ReturnValue copy_from(const IAny &other) override
    {
        if (is_compatible(other, TYPE_UID)) {
            T value;
            if (succeeded(other.get_data(&value, sizeof(T), TYPE_UID))) {
                return set_value(value);
            }
        }
        return ReturnValue::FAIL;
    }

private:
    static constexpr bool is_valid_args(void const *to, size_t toSize, Uid type)
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
template<class T>
class AnyValue final : public AnyCore<AnyValue<T>, T>
{
    using Base = AnyCore<AnyValue<T>, T>;

public:
    ReturnValue set_value(const T &value) override
    {
        if constexpr (std::is_trivially_copyable_v<T>) {
            if (std::memcmp(&data_, &value, sizeof(T)) != 0) {
                std::memcpy(&data_, &value, sizeof(T));
                return ReturnValue::SUCCESS;
            }
        } else {
            if (data_ != value) {
                data_ = value;
                return ReturnValue::SUCCESS;
            }
        }
        return ReturnValue::NOTHING_TO_DO;
    }

    const T& get_value() const override { return data_; }

    size_t get_data_size(Uid type) const override
    {
        return type == Base::TYPE_UID ? sizeof(T) : 0;
    }

    ReturnValue get_data(void *to, size_t toSize, Uid type) const override
    {
        if (!valid_args(to, toSize, type)) {
            return ReturnValue::FAIL;
        }
        if constexpr (std::is_trivially_copyable_v<T>) {
            std::memcpy(to, &data_, sizeof(T));
        } else {
            *reinterpret_cast<T *>(to) = data_;
        }
        return ReturnValue::SUCCESS;
    }

    ReturnValue set_data(const void *from, size_t fromSize, Uid type) override
    {
        return valid_args(from, fromSize, type)
            ? set_value(*reinterpret_cast<const T *>(from))
            : ReturnValue::FAIL;
    }

    ReturnValue copy_from(const IAny &other) override
    {
        if (!is_compatible(other, Base::TYPE_UID)) {
            return ReturnValue::FAIL;
        }
        if constexpr (std::is_trivially_copyable_v<T>) {
            char buf[sizeof(T)];
            return succeeded(other.get_data(buf, sizeof(T), Base::TYPE_UID))
                ? set_value(*reinterpret_cast<const T *>(buf)) : ReturnValue::FAIL;
        } else {
            T value{};
            return succeeded(other.get_data(&value, sizeof(T), Base::TYPE_UID))
                ? set_value(value) : ReturnValue::FAIL;
        }
    }

private:
    static constexpr bool valid_args(const void *p, size_t size, Uid type)
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
template<class T>
class AnyRef final : public AnyCore<AnyRef<T>, T>
{
    using Base = AnyCore<AnyRef<T>, T>;

public:
    explicit AnyRef(T* ptr = nullptr) : ptr_(ptr) {}

    /** @brief Retargets this any to a different memory location. */
    void set_target(T* ptr) { ptr_ = ptr; }

    ReturnValue set_value(const T &value) override
    {
        if constexpr (std::is_trivially_copyable_v<T>) {
            if (std::memcmp(ptr_, &value, sizeof(T)) != 0) {
                std::memcpy(ptr_, &value, sizeof(T));
                return ReturnValue::SUCCESS;
            }
        } else {
            if (*ptr_ != value) {
                *ptr_ = value;
                return ReturnValue::SUCCESS;
            }
        }
        return ReturnValue::NOTHING_TO_DO;
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

/** @brief Creates an AnyRef<T> wrapped in a shared_ptr, pointing to the given address. */
template<class T>
IAny::Ptr create_any_ref(T* ptr) {
    auto* obj = new AnyRef<T>(ptr);
    return IAny::Ptr(static_cast<IAny*>(obj));
}

} // namespace velk::ext

#endif // ANY_H
