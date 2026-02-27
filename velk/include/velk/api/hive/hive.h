#ifndef VELK_API_HIVE_H
#define VELK_API_HIVE_H

#include <velk/api/hive/object_hive.h>
#include <velk/api/hive/raw_hive.h>

namespace velk {

/**
 * @brief Creates an ObjectHive from a hive store.
 * @tparam T The object class to store (must have class_id()).
 * @tparam I The implicit interface type for hive objects.
 */
template <class T, class I = IObject>
ObjectHive<I> create_hive(IHiveStore& store)
{
    return ObjectHive<>(store.get_hive<T>());
}

/**
 * @brief Creates an ObjectHive for the given class UID from a hive store.
 * @tparam I The implicit interface type for hive objects.
 */
template <class I = IObject>
ObjectHive<I> create_hive(IHiveStore& store, Uid classUid)
{
    return ObjectHive<>(store.get_hive(classUid));
}

/**
 * @brief Creates a RawHive from a hive store.
 * @tparam T The element type to store (must not have class_id()).
 */
template <class T>
RawHive<T> create_raw_hive(IHiveStore& store)
{
    return RawHive<T>(store.get_raw_hive<T>());
}

} // namespace velk

#endif // VELK_API_HIVE_H
