#include "hive_registry.h"

#include "hive.h"

#include <velk/api/velk.h>

#include <algorithm>

namespace velk {

IHive::Ptr HiveRegistry::get_hive(Uid classUid)
{
    HiveEntry key{classUid, {}};
    auto it = std::lower_bound(hives_.begin(), hives_.end(), key);
    if (it != hives_.end() && it->uid == classUid) {
        return it->hive;
    }

    auto& velk = instance();

    // Verify that the class UID is registered.
    if (!velk.type_registry().get_class_info(classUid)) {
        return {};
    }

    // Create and initialize a new hive for this class UID.
    auto hive_obj = ext::make_object<Hive>();
    auto* hive = static_cast<Hive*>(hive_obj.get());
    hive->init(classUid);
    auto hive_ptr = interface_pointer_cast<IHive>(hive_obj);

    hives_.insert(it, HiveEntry{classUid, hive_ptr});
    return hive_ptr;
}

IHive::Ptr HiveRegistry::find_hive(Uid classUid) const
{
    HiveEntry key{classUid, {}};
    auto it = std::lower_bound(hives_.begin(), hives_.end(), key);
    if (it != hives_.end() && it->uid == classUid) {
        return it->hive;
    }
    return {};
}

size_t HiveRegistry::hive_count() const
{
    return hives_.size();
}

void HiveRegistry::for_each_hive(void* context, HiveVisitorFn visitor) const
{
    for (auto& entry : hives_) {
        if (!visitor(context, *entry.hive)) {
            return;
        }
    }
}

} // namespace velk
