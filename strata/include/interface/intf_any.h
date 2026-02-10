#ifndef INTF_ANY_H
#define INTF_ANY_H

#include <common.h>
//#include <interface/intf_event.h>
#include <interface/intf_interface.h>
#include <interface/types.h>

namespace strata {

/**
 * @brief Type-erased value container interface.
 *
 * Supports querying compatible types, and getting/setting data by type UID.
 */
class IAny : public Interface<IAny>
{
public:
    /**
     * @brief Returns a list of types this IAny is compatible with.
     */
    virtual const std::vector<Uid> &GetCompatibleTypes() const = 0;
    /**
     * @brief Returns the size of data contained within the any
     * @param type The type whose data size to query. Must be one of the values returned by GetCompatibleTypes.
     */
    virtual size_t GetDataSize(Uid type) const = 0;
    /**
     * @brief Stores the data contained in this IAny to destination buffer.
     * @param to The buffer to store the data to.
     * @param toSize Size of the buffer.
     * @param type Type which should be stored. Must be one of the values returned by GetCompatibleTypes.
     * @return SUCCESS if data was successfully stored, FAIL otherwise.
     */
    virtual ReturnValue GetData(void *to, size_t toSize, Uid type) const = 0;
    /**
     * @brief Stores a given data to this IAny.
     * @param from The buffer to read data from.
     * @param fromSize Size of the buffer.
     * @param type Type of the data. Must be one of the values returned by GetCompatibleTypes.
     * @return SUCCESS if the value was set successfully and changed as a result of the operation
     *         NOTHING_TO_DO if the operation was successful but did not result to the contained value changing.
     *         FAIL otherwise.
     */
    virtual ReturnValue SetData(void const *from, size_t fromSize, Uid type) = 0;
    /**
     * @brief Copies the content of this IAny from another IAny.
     * @param other The IAny to copy from. Must be compatible with this IAny.
     * @return SUCCESS if copy operation completed successfully, FAIL otherwise.
     */
    virtual ReturnValue CopyFrom(const IAny &other) = 0;
};

/**
 * @brief Returns true if any is compatible with given type.
 * @param any The IAny whose compatibility to check.
 * @param type The type to check against.
 */
inline bool IsCompatible(const IAny &any, Uid type)
{
    for (auto &&uid : any.GetCompatibleTypes()) {
        if (uid == type) {
            return true;
        }
    }
    return false;
}

/** @copydoc IsCompatible(const IAny&, Uid) */
inline bool IsCompatible(const IAny::ConstPtr &any, Uid type)
{
    return any && IsCompatible(*(any.get()), type);
}

/** @brief Returns true if the any is compatible with type T. */
template<class T>
inline bool IsCompatible(const IAny::ConstPtr &any)
{
    return IsCompatible(any, TypeUid<T>());
}

/** @copydoc IsCompatible(const IAny&, Uid) */
inline bool IsCompatible(const IAny::RefPtr &any, Uid type)
{
    return any && IsCompatible(*(any.get()), type);
}

/**
 * @brief Returns the first type UID that both any objects are compatible with, or 0 if none.
 */
inline Uid GetCompatibleType(const IAny::ConstPtr &any, const IAny::ConstPtr &with)
{
    if (any && with) {
        for (auto &&uid : any->GetCompatibleTypes()) {
            if (IsCompatible(with, uid)) {
                return uid;
            }
        }
    }
    return {};
}

/** @brief Returns true if two any objects share at least one compatible type. */
inline bool IsCompatible(const IAny::ConstPtr &any, const IAny::ConstPtr &with)
{
    return GetCompatibleType(any, with) != Uid{};
}

} // namespace strata

#endif
