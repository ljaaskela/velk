#include "hive_store.h"

#include "object_hive.h"
#include "raw_hive.h"

#include <velk/api/velk.h>

#include <algorithm>

namespace velk {

IObjectHive::Ptr HiveStore::get_hive(Uid classUid)
{
    HiveEntry key{classUid, {}};
    auto it = std::lower_bound(hives_.begin(), hives_.end(), key);
    if (it != hives_.end() && it->uid == classUid) {
        return interface_pointer_cast<IObjectHive>(it->hive);
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
    auto hive_ptr = interface_pointer_cast<IHive>(hive_obj);

    hives_.insert(it, HiveEntry{classUid, hive_ptr});
    return interface_pointer_cast<IObjectHive>(hive_ptr);
}

IObjectHive::Ptr HiveStore::find_hive(Uid classUid) const
{
    HiveEntry key{classUid, {}};
    auto it = std::lower_bound(hives_.begin(), hives_.end(), key);
    if (it != hives_.end() && it->uid == classUid) {
        return interface_pointer_cast<IObjectHive>(it->hive);
    }
    return {};
}

IRawHive::Ptr HiveStore::get_raw_hive(Uid uid, size_t element_size, size_t element_align)
{
    HiveEntry key{uid, {}};
    auto it = std::lower_bound(hives_.begin(), hives_.end(), key);
    if (it != hives_.end() && it->uid == uid) {
        return interface_pointer_cast<IRawHive>(it->hive);
    }

    // Create and initialize a new raw hive.
    auto hive_obj = ext::make_object<RawHiveImpl>();
    auto* hive = static_cast<RawHiveImpl*>(hive_obj.get());
    hive->init(uid, element_size, element_align);
    auto hive_ptr = interface_pointer_cast<IHive>(hive_obj);

    hives_.insert(it, HiveEntry{uid, hive_ptr});
    return interface_pointer_cast<IRawHive>(hive_ptr);
}

IRawHive::Ptr HiveStore::find_raw_hive(Uid uid) const
{
    HiveEntry key{uid, {}};
    auto it = std::lower_bound(hives_.begin(), hives_.end(), key);
    if (it != hives_.end() && it->uid == uid) {
        return interface_pointer_cast<IRawHive>(it->hive);
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
        auto* hive = interface_cast<IHive>(entry.hive);
        if (hive && !visitor(context, *hive)) {
            return;
        }
    }
}

} // namespace velk
