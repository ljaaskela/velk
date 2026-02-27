#ifndef VELK_INTF_HIVE_STORE_H
#define VELK_INTF_HIVE_STORE_H

#include <velk/ext/member_traits.h>
#include <velk/interface/hive/intf_hive.h>

namespace velk {

namespace ClassId {
/** @brief Store managing hives, one per class ID. */
inline constexpr Uid HiveStore{"7a383dd3-a3cf-48f2-a8ac-4da38df5bb13"};
} // namespace ClassId

/**
 * @brief Interface for managing hives keyed by UID.
 *
 * Provides lazy creation and lookup of both object hives (ref-counted
 * Velk objects) and raw hives (plain data allocations).
 */
class IHiveStore : public Interface<IHiveStore>
{
public:
    /** @brief Visitor for iterating all hives (both object and raw). */
    using HiveVisitorFn = bool (*)(void* context, IHive& hive);

    /** @brief Returns the object hive for the given class UID, creating it if it does not exist. */
    virtual IObjectHive::Ptr get_hive(Uid classUid) = 0;

    /** @brief Returns the object hive for the given class UID, or nullptr if it does not exist. */
    virtual IObjectHive::Ptr find_hive(Uid classUid) const = 0;

    /** @brief Returns the raw hive for the given UID, creating it if it does not exist. */
    virtual IRawHive::Ptr get_raw_hive(Uid uid, size_t element_size, size_t element_align) = 0;

    /** @brief Returns the raw hive for the given UID, or nullptr if it does not exist. */
    virtual IRawHive::Ptr find_raw_hive(Uid uid) const = 0;

    /** @brief Returns the number of active hives (object + raw). */
    virtual size_t hive_count() const = 0;

    /**
     * @brief Iterates all active hives (object and raw).
     * @param context Opaque pointer forwarded to the visitor.
     * @param visitor Called for each hive. Return false to stop early.
     */
    virtual void for_each_hive(void* context, HiveVisitorFn visitor) const = 0;

    /** @brief Returns the object hive for type T, creating it if it does not exist. */
    template <class T>
    IObjectHive::Ptr get_hive()
    {
        static_assert(detail::has_class_id<T>::value,
                      "T must be a registered Velk object type with class_id(). "
                      "For plain data types use get_raw_hive<T>() instead.");
        return get_hive(T::class_id());
    }

    /** @brief Returns the object hive for type T, or nullptr if it does not exist. */
    template <class T>
    IObjectHive::Ptr find_hive() const
    {
        static_assert(detail::has_class_id<T>::value,
                      "T must be a registered Velk object type with class_id(). "
                      "For plain data types use find_raw_hive<T>() instead.");
        return find_hive(T::class_id());
    }

    /** @brief Returns the raw hive for type T, creating it if it does not exist. */
    template <class T>
    IRawHive::Ptr get_raw_hive()
    {
        static_assert(!detail::has_class_id<T>::value,
                      "T is a Velk object type. Use get_hive<T>() for ref-counted objects, "
                      "or get_raw_hive(type_uid<T>(), ...) if you really want raw allocation.");
        return get_raw_hive(type_uid<T>(), sizeof(T), alignof(T));
    }

    /** @brief Returns the raw hive for type T, or nullptr if it does not exist. */
    template <class T>
    IRawHive::Ptr find_raw_hive() const
    {
        static_assert(!detail::has_class_id<T>::value,
                      "T is a Velk object type. Use find_hive<T>() for ref-counted objects, "
                      "or find_raw_hive(type_uid<T>()) if you really want raw lookup.");
        return find_raw_hive(type_uid<T>());
    }
};

} // namespace velk

#endif // VELK_INTF_HIVE_STORE_H
