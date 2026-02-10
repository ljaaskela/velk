#ifndef EXT_ANY_H
#define EXT_ANY_H

#include <common.h>
#include <ext/core_object.h>
#include <interface/intf_any.h>
#include <interface/intf_strata.h>

#include <algorithm>

namespace strata {

/**
 * @brief Base class for IAny implementations built on the Object system.
 * @tparam FinalClass The final derived class (CRTP parameter).
 * @tparam Interfaces Additional interfaces beyond IAny.
 */
template<class FinalClass, class... Interfaces>
class BaseAny : public CoreObject<FinalClass, IAny, Interfaces...>
{
public:
    static constexpr const std::string_view GetClassName() { return GetName<FinalClass>(); }
    static constexpr Uid GetClassUid() { return {}; }
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
    static constexpr Uid TYPE_UID = TypeUid<Types...>();

    const std::vector<Uid> &GetCompatibleTypes() const override
    {
        static vector<Uid> uids = {(TypeUid<Types>())...};
        return uids;
    }

    static constexpr Uid GetClassUid() { return TYPE_UID; }
};

/**
 * @brief A helper template for implementing an Any which supports a single type
 */
template<class FinalClass, class T, class... Interfaces>
class SingleTypeAny : public BaseAny<FinalClass, Interfaces...>
{
public:
    static constexpr Uid TYPE_UID = TypeUid<T>();
    virtual ReturnValue Set(const T &value) = 0;
    virtual const T &Get() const = 0;

    static constexpr Uid GetClassUid() { return TYPE_UID; }

    const std::vector<Uid> &GetCompatibleTypes() const override
    {
        static vector<Uid> uids = {TYPE_UID};
        return uids;
    }
    size_t GetDataSize(Uid type) const override final
    {
        if (type == TYPE_UID) {
            return sizeof(T);
        }
        return 0;
    }
    ReturnValue GetData(void *to, size_t toSize, Uid type) const override
    {
        if (IsValidArgs(to, toSize, type)) {
            *reinterpret_cast<T *>(to) = Get();
            return ReturnValue::SUCCESS;
        }
        return ReturnValue::FAIL;
    }
    ReturnValue SetData(void const *from, size_t fromSize, Uid type) override final
    {
        if (IsValidArgs(from, fromSize, type)) {
            return Set(*reinterpret_cast<const T *>(from));
        }
        return ReturnValue::FAIL;
    }
    ReturnValue CopyFrom(const IAny &other) override
    {
        if (IsCompatible(other, TYPE_UID)) {
            T value;
            if (Succeeded(other.GetData(&value, sizeof(T), TYPE_UID))) {
                return Set(value);
            }
        }
        return ReturnValue::FAIL;
    }

private:
    static constexpr bool IsValidArgs(void const *to, size_t toSize, Uid type)
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
    virtual ReturnValue Set(const T &value)
    {
        if (data_ != value) {
            data_ = value;
            return ReturnValue::SUCCESS;
        }
        return ReturnValue::NOTHING_TO_DO;
    }
    virtual const T& Get() const
    {
        return data_;
    }

private:
    T data_;
};

} // namespace strata

#endif // ANY_H
