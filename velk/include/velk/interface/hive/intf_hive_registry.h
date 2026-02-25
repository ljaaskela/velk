#ifndef VELK_INTF_HIVE_REGISTRY_H
#define VELK_INTF_HIVE_REGISTRY_H

#include <velk/interface/hive/intf_hive.h>

namespace velk {

namespace ClassId {
/** @brief Built-in plugin that provides IHiveRegistry. */
inline constexpr Uid HivePlugin{"c32cf115-ddb4-48c2-b683-46a02d148ec4"};
/** @brief Registry managing hives, one per class ID. */
inline constexpr Uid HiveRegistry{"7a383dd3-a3cf-48f2-a8ac-4da38df5bb13"};
} // namespace ClassId

/**
 * @brief Interface for managing hives, one per class ID.
 *
 * Provides lazy creation of hives (via get_hive) and lookup of existing
 * hives (via find_hive). Typically owned by HivePlugin.
 */
class IHiveRegistry : public Interface<IHiveRegistry>
{
public:
    /** @brief Returns the hive for the given class UID, creating it if it does not exist. */
    virtual IHive::Ptr get_hive(Uid classUid) = 0;

    /** @brief Returns the hive for the given class UID, or nullptr if it does not exist. */
    virtual IHive::Ptr find_hive(Uid classUid) const = 0;

    /** @brief Returns the number of active hives. */
    virtual size_t hive_count() const = 0;

    /**
     * @brief Iterates all active hives.
     * @param context Opaque pointer forwarded to the visitor.
     * @param visitor Called for each hive. Return false to stop early.
     */
    using HiveVisitorFn = bool (*)(void* context, IHive& hive);
    virtual void for_each_hive(void* context, HiveVisitorFn visitor) const = 0;

    /** @brief Returns the hive for type T, creating it if it does not exist. */
    template <class T>
    IHive::Ptr get_hive()
    {
        return get_hive(T::class_id());
    }

    /** @brief Returns the hive for type T, or nullptr if it does not exist. */
    template <class T>
    IHive::Ptr find_hive() const
    {
        return find_hive(T::class_id());
    }
};

} // namespace velk

#endif // VELK_INTF_HIVE_REGISTRY_H
