#ifndef VELK_PLUGINS_HIVE_REGISTRY_H
#define VELK_PLUGINS_HIVE_REGISTRY_H

#include <velk/ext/core_object.h>
#include <velk/interface/hive/intf_hive_registry.h>

#include <vector>

namespace velk {

/**
 * @brief Concrete implementation of IHiveRegistry.
 *
 * Maintains a sorted vector of hives keyed by class UID. Hives are lazily
 * created on first access via get_hive().
 */
class HiveRegistry final : public ext::ObjectCore<HiveRegistry, IHiveRegistry>
{
public:
    VELK_CLASS_UID(ClassId::HiveRegistry);

    // IHiveRegistry overrides
    IHive::Ptr get_hive(Uid classUid) override;
    IHive::Ptr find_hive(Uid classUid) const override;
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

#endif // VELK_PLUGINS_HIVE_REGISTRY_H
