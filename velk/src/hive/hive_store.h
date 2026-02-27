#ifndef VELK_PLUGINS_HIVE_STORE_H
#define VELK_PLUGINS_HIVE_STORE_H

#include <velk/ext/core_object.h>
#include <velk/interface/hive/intf_hive_store.h>

#include <vector>

namespace velk {

/**
 * @brief Concrete implementation of IHiveStore.
 *
 * Maintains a sorted vector of hives keyed by UID. Hives are lazily
 * created on first access via get_hive() or get_raw_hive().
 */
class HiveStore final : public ext::ObjectCore<HiveStore, IHiveStore>
{
public:
    VELK_CLASS_UID(ClassId::HiveStore);

    // IHiveStore overrides
    IObjectHive::Ptr get_hive(Uid classUid) override;
    IObjectHive::Ptr find_hive(Uid classUid) const override;
    IRawHive::Ptr get_raw_hive(Uid uid, size_t element_size, size_t element_align) override;
    IRawHive::Ptr find_raw_hive(Uid uid) const override;
    size_t hive_count() const override;
    void for_each_hive(void* context, HiveVisitorFn visitor) const override;

private:
    struct HiveEntry
    {
        Uid uid;
        IHive::Ptr hive;
        bool operator<(const HiveEntry& o) const { return uid < o.uid; }
    };

    std::vector<HiveEntry> hives_;
};

} // namespace velk

#endif // VELK_PLUGINS_HIVE_STORE_H
