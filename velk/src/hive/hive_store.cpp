#include "hive_store.h"

#include "object_hive.h"

#include <velk/api/velk.h>

#include <algorithm>

namespace velk {

IObjectHive::Ptr HiveStore::get_hive(Uid classUid)
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
    auto hive_obj = ext::make_object<ObjectHive>();
    auto* hive = static_cast<ObjectHive*>(hive_obj.get());
    hive->init(classUid);
    auto hive_ptr = interface_pointer_cast<IObjectHive>(hive_obj);

    hives_.insert(it, HiveEntry{classUid, hive_ptr});
    return hive_ptr;
}

IObjectHive::Ptr HiveStore::find_hive(Uid classUid) const
{
    HiveEntry key{classUid, {}};
    auto it = std::lower_bound(hives_.begin(), hives_.end(), key);
    if (it != hives_.end() && it->uid == classUid) {
        return it->hive;
    }
    return {};
}

size_t HiveStore::hive_count() const
{
    return hives_.size();
}

void HiveStore::for_each_hive(void* context, HiveVisitorFn visitor) const
{
    for (auto& entry : hives_) {
        if (!visitor(context, *entry.hive)) {
            return;
        }
    }
}

} // namespace velk
