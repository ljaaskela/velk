#ifndef EXT_ANY_H
#define EXT_ANY_H

#include <common.h>
#include <ext/core_object.h>
#include <interface/intf_any.h>
#include <interface/intf_strata.h>

#include <algorithm>

namespace strata {

/**
 * @brief Base class for IAny implementations.
 *
 * Inherits RefCountedDispatch directly with IObject (no ISharedFromObject or self-pointer).
 *
 * @tparam FinalClass The final derived class (CRTP parameter).
 * @tparam Interfaces Additional interfaces beyond IAny.
 */
template<class FinalClass, class... Interfaces>
class BaseAny : public RefCountedDispatch<IAny, Interfaces...>
{
public:
    /** @brief Returns the compile-time class name of FinalClass. */
    static constexpr std::string_view get_class_name() { return get_name<FinalClass>(); }
    /** @brief Returns a default UID (overridden by typed subclasses). */
    static constexpr Uid get_class_uid() { return {}; }

    /** @brief Returns the singleton factory for creating instances of FinalClass. */
    static const IObjectFactory &get_factory()
    {
        static Factory factory_;
        return factory_;
    }

private:
    class Factory : public ObjectFactory<FinalClass>
    {
        const ClassInfo &get_class_info() const override
        {
            static constexpr ClassInfo info{FinalClass::get_class_uid(), FinalClass::get_class_name()};
            return info;
        }
    };
};

/**
 * @brief BaseAny specialization that declares compatibility with one or more types.
 * @tparam FinalClass The final derived class (CRTP parameter).
 * @tparam Types The data types this any is compatible with.
 */
template<class FinalClass, class... Types>
class BaseAnyT : public BaseAny<FinalClass>
{
public:
    static constexpr Uid TYPE_UID = type_uid<Types...>();

    /** @brief Returns the list of type UIDs this any is compatible with. */
    const std::vector<Uid> &get_compatible_types() const override
    {
        static std::vector<Uid> uids = {(type_uid<Types>())...};
        return uids;
    }

    /** @brief Returns the UID for the combined type pack. */
    static constexpr Uid get_class_uid() { return TYPE_UID; }
};

/**
 * @brief A helper template for implementing an Any which supports a single type
 */
template<class FinalClass, class T, class... Interfaces>
class SingleTypeAny : public BaseAny<FinalClass, Interfaces...>
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
    const std::vector<Uid> &get_compatible_types() const override
    {
        static std::vector<Uid> uids = {TYPE_UID};
        return uids;
    }
    size_t get_data_size(Uid type) const override final
    {
        if (type == TYPE_UID) {
            return sizeof(T);
        }
        return 0;
    }
    ReturnValue get_data(void *to, size_t toSize, Uid type) const override
    {
        if (is_valid_args(to, toSize, type)) {
            *reinterpret_cast<T *>(to) = get_value();
            return ReturnValue::SUCCESS;
        }
        return ReturnValue::FAIL;
    }
    ReturnValue set_data(void const *from, size_t fromSize, Uid type) override final
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
 * @brief A basic Any implementation with a single supported data type which is stored in local storage
 */
template<class T>
class SimpleAny final : public SingleTypeAny<SimpleAny<T>, T>
{
    virtual ReturnValue set_value(const T &value)
    {
        if (data_ != value) {
            data_ = value;
            return ReturnValue::SUCCESS;
        }
        return ReturnValue::NOTHING_TO_DO;
    }
    virtual const T& get_value() const
    {
        return data_;
    }

private:
    T data_;
};

} // namespace strata

#endif // ANY_H
