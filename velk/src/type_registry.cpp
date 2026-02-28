#include "type_registry.h"

#include "array_property.h"
#include "function.h"
#include "future.h"
#include "hive/hive_store.h"
#include "hive/object_hive.h"
#include "hive/raw_hive.h"
#include "property.h"

#include <velk/ext/any.h>
#include <velk/interface/intf_log.h>
#include <velk/string.h>

#include <algorithm>

namespace velk {

TypeRegistry::TypeRegistry(ILog& log) : log_(log)
{
    ITypeRegistry::register_type<PropertyImpl>();
    ITypeRegistry::register_type<ArrayPropertyImpl>();
    ITypeRegistry::register_type<FunctionImpl>();
    ITypeRegistry::register_type<FutureImpl>();
    ITypeRegistry::register_type<HiveStore>();
    ITypeRegistry::register_type<ObjectHive>();
    ITypeRegistry::register_type<RawHiveImpl>();

    ITypeRegistry::register_type<ext::AnyValue<float>>();
    ITypeRegistry::register_type<ext::AnyValue<double>>();
    ITypeRegistry::register_type<ext::AnyValue<uint8_t>>();
    ITypeRegistry::register_type<ext::AnyValue<uint16_t>>();
    ITypeRegistry::register_type<ext::AnyValue<uint32_t>>();
    ITypeRegistry::register_type<ext::AnyValue<uint64_t>>();
    ITypeRegistry::register_type<ext::AnyValue<int8_t>>();
    ITypeRegistry::register_type<ext::AnyValue<int16_t>>();
    ITypeRegistry::register_type<ext::AnyValue<int32_t>>();
    ITypeRegistry::register_type<ext::AnyValue<int64_t>>();
    ITypeRegistry::register_type<ext::AnyValue<string>>();

    ITypeRegistry::register_type<ext::ArrayAnyValue<float>>();
    ITypeRegistry::register_type<ext::ArrayAnyValue<double>>();
    ITypeRegistry::register_type<ext::ArrayAnyValue<uint8_t>>();
    ITypeRegistry::register_type<ext::ArrayAnyValue<uint16_t>>();
    ITypeRegistry::register_type<ext::ArrayAnyValue<uint32_t>>();
    ITypeRegistry::register_type<ext::ArrayAnyValue<uint64_t>>();
    ITypeRegistry::register_type<ext::ArrayAnyValue<int8_t>>();
    ITypeRegistry::register_type<ext::ArrayAnyValue<int16_t>>();
    ITypeRegistry::register_type<ext::ArrayAnyValue<int32_t>>();
    ITypeRegistry::register_type<ext::ArrayAnyValue<int64_t>>();
    ITypeRegistry::register_type<ext::ArrayAnyValue<string>>();
}

const IObjectFactory* TypeRegistry::find(Uid uid) const
{
    Entry key{uid, nullptr};
    auto it = std::lower_bound(types_.begin(), types_.end(), key);
    if (it != types_.end() && it->uid == uid) {
        return it->factory;
    }
    return nullptr;
}

ReturnValue TypeRegistry::register_type(const IObjectFactory& factory)
{
    auto& info = factory.get_class_info();
    detail::velk_log(log_,
                     LogLevel::Debug,
                     __FILE__,
                     __LINE__,
                     "Register %.*s",
                     static_cast<int>(info.name.size()),
                     info.name.data());
    Entry entry{info.uid, &factory, current_owner_};
    auto it = std::lower_bound(types_.begin(), types_.end(), entry);
    if (it != types_.end() && it->uid == info.uid) {
        it->factory = &factory;
        it->owner = current_owner_;
    } else {
        types_.insert(it, entry);
    }
    return ReturnValue::Success;
}

ReturnValue TypeRegistry::unregister_type(const IObjectFactory& factory)
{
    Entry key{factory.get_class_info().uid, nullptr};
    auto it = std::lower_bound(types_.begin(), types_.end(), key);
    if (it != types_.end() && it->uid == key.uid) {
        types_.erase(it);
    }
    return ReturnValue::Success;
}

IInterface::Ptr TypeRegistry::create(Uid uid, uint32_t flags) const
{
    if (auto* factory = find(uid)) {
        return factory->create_instance(flags);
    }
    return {};
}

const ClassInfo* TypeRegistry::get_class_info(Uid classUid) const
{
    if (auto* factory = find(classUid)) {
        return &factory->get_class_info();
    }
    return nullptr;
}

const IObjectFactory* TypeRegistry::find_factory(Uid classUid) const
{
    return find(classUid);
}

void TypeRegistry::set_owner(Uid uid)
{
    current_owner_ = uid;
}

void TypeRegistry::sweep_owner(Uid uid)
{
    types_.erase(std::remove_if(types_.begin(), types_.end(), [&](const Entry& e) { return e.owner == uid; }),
                 types_.end());
}

} // namespace velk
