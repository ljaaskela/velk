#ifndef VELK_INTF_HIVE_STORE_H
#define VELK_INTF_HIVE_STORE_H

#include <velk/interface/hive/intf_hive.h>

namespace velk {

namespace ClassId {
/** @brief Store managing hives, one per class ID. */
inline constexpr Uid HiveStore{"7a383dd3-a3cf-48f2-a8ac-4da38df5bb13"};
} // namespace ClassId

/**
 * @brief Interface for managing hives, one per class ID.
 *
 * Provides lazy creation of hives (via get_hive) and lookup of existing
 * hives (via find_hive).
 */
class IHiveStore : public Interface<IHiveStore>
{
public:
    /** @brief Returns the hive for the given class UID, creating it if it does not exist. */
    virtual IObjectHive::Ptr get_hive(Uid classUid) = 0;

    /** @brief Returns the hive for the given class UID, or nullptr if it does not exist. */
    virtual IObjectHive::Ptr find_hive(Uid classUid) const = 0;

    /** @brief Returns the number of active hives. */
    virtual size_t hive_count() const = 0;

    /**
     * @brief Iterates all active hives.
     * @param context Opaque pointer forwarded to the visitor.
     * @param visitor Called for each hive. Return false to stop early.
     */
    using HiveVisitorFn = bool (*)(void* context, IObjectHive& hive);
    virtual void for_each_hive(void* context, HiveVisitorFn visitor) const = 0;

    /** @brief Returns the hive for type T, creating it if it does not exist. */
    template <class T>
    IObjectHive::Ptr get_hive()
    {
        return get_hive(T::class_id());
    }

    /** @brief Returns the hive for type T, or nullptr if it does not exist. */
    template <class T>
    IObjectHive::Ptr find_hive() const
    {
        return find_hive(T::class_id());
    }
};

} // namespace velk

#endif // VELK_INTF_HIVE_STORE_H
