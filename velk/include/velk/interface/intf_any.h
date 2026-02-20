#ifndef VELK_INTF_ANY_H
#define VELK_INTF_ANY_H

#include <velk/array_view.h>
#include <velk/common.h>
#include <velk/interface/intf_object.h>
#include <velk/interface/types.h>

namespace velk {

/**
 * @brief Type-erased value container interface.
 *
 * Inherits IObject so that Any types are factory-compatible without
 * needing the full ObjectCore machinery (metadata).
 *
 * Supports querying compatible types, and getting/setting data by type UID.
 */
class IAny : public Interface<IAny, IObject>
{
public:
    /**
     * @brief Returns a list of types this IAny is compatible with.
     */
    virtual array_view<Uid> get_compatible_types() const = 0;
    /**
     * @brief Returns the size of data contained within the any
     * @param type The type whose data size to query. Must be one of the values returned by get_compatible_types.
     */
    virtual size_t get_data_size(Uid type) const = 0;
    /**
     * @brief Stores the data contained in this IAny to destination buffer.
     * @param to The buffer to store the data to.
     * @param toSize Size of the buffer.
     * @param type Type which should be stored. Must be one of the values returned by get_compatible_types.
     * @return SUCCESS if data was successfully stored, FAIL otherwise.
     */
    virtual ReturnValue get_data(void *to, size_t toSize, Uid type) const = 0;
    /**
     * @brief Stores a given data to this IAny.
     * @param from The buffer to read data from.
     * @param fromSize Size of the buffer.
     * @param type Type of the data. Must be one of the values returned by get_compatible_types.
     * @return SUCCESS if the value was set successfully and changed as a result of the operation
     *         NOTHING_TO_DO if the operation was successful but did not result to the contained value changing.
     *         FAIL otherwise.
     */
    virtual ReturnValue set_data(void const *from, size_t fromSize, Uid type) = 0;
    /**
     * @brief Copies the content of this IAny from another IAny.
     * @param other The IAny to copy from. Must be compatible with this IAny.
     * @return SUCCESS if copy operation completed successfully, FAIL otherwise.
     */
    virtual ReturnValue copy_from(const IAny &other) = 0;
    /**
     * @brief Creates a new IAny of the same type and copies the contained value.
     * @return A new IAny with the same data, or null if cloning fails.
     */
    virtual IAny::Ptr clone() const = 0;
};

/**
 * @brief Returns true if any is compatible with given type.
 * @param any The IAny whose compatibility to check.
 * @param type The type to check against.
 */
inline bool is_compatible(const IAny &any, Uid type)
{
    for (auto &&uid : any.get_compatible_types()) {
        if (uid == type) {
            return true;
        }
    }
    return false;
}

/** @copydoc is_compatible(const IAny&, Uid) */
inline bool is_compatible(const IAny::ConstPtr &any, Uid type)
{
    return any && is_compatible(*(any.get()), type);
}

/** @brief Returns true if the any is compatible with type T. */
template<class T>
inline bool is_compatible(const IAny::ConstPtr &any)
{
    return is_compatible(any, type_uid<T>());
}

/** @copydoc is_compatible(const IAny&, Uid) */
inline bool is_compatible(const IAny::RefPtr &any, Uid type)
{
    return any && is_compatible(*(any.get()), type);
}

/**
 * @brief Returns the first type UID that both any objects are compatible with, or 0 if none.
 */
inline Uid get_compatible_type(const IAny::ConstPtr &any, const IAny::ConstPtr &with)
{
    if (any && with) {
        for (auto &&uid : any->get_compatible_types()) {
            if (is_compatible(with, uid)) {
                return uid;
            }
        }
    }
    return {};
}

/** @brief Returns true if two any objects share at least one compatible type. */
inline bool is_compatible(const IAny::ConstPtr &any, const IAny::ConstPtr &with)
{
    return get_compatible_type(any, with) != Uid{};
}

} // namespace velk

#endif
