#include "type_registry.h"

#include "array_property.h"
#include "binding.h"
#include "event.h"
#include "function.h"
#include "future.h"
#include "generic_object.h"
#include "hierarchy.h"
#include "hive/hive_store.h"
#include "hive/raw_hive.h"
#include "object_ref.h"
#include "property.h"
#include "standalone_property.h"
#include "store.h"
#include "thread_context.h"
#include "variant.h"

#include <velk/api/math_types.h>
#include <velk/api/velk.h>
#include <velk/ext/any.h>
#include <velk/interface/hive/intf_hive.h>
#include <velk/interface/intf_log.h>
#include <velk/interface/intf_velk.h>
#include <velk/string.h>

#include <algorithm>
#include <shared_mutex>

namespace velk {

TypeRegistry::TypeRegistry(ILog& log) : log_(log)
{
    ITypeRegistry::register_type<impl::Property>();
    ITypeRegistry::register_type<impl::ArrayProperty>();
    ITypeRegistry::register_type<impl::Function>();
    ITypeRegistry::register_type<impl::Event>();
    ITypeRegistry::register_type<impl::Future>();
    ITypeRegistry::register_type<impl::HiveStore>();
    ITypeRegistry::register_type<impl::ObjectHive>();
    ITypeRegistry::register_type<impl::RawHive>();
    ITypeRegistry::register_type<impl::Hierarchy>();
    ITypeRegistry::register_type<impl::Binding>();
    ITypeRegistry::register_type<impl::Variant>();
    ITypeRegistry::register_type<impl::ObjectRef>();
    ITypeRegistry::register_type<impl::Store>();
    ITypeRegistry::register_type<impl::ThreadContext>();
    ITypeRegistry::register_type<impl::GenericObject>();

    ITypeRegistry::register_type<ext::AnyValue<bool>>();
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
    ITypeRegistry::register_type<ext::AnyValue<Duration>>();
    ITypeRegistry::register_type<ext::AnyValue<vec2>>();
    ITypeRegistry::register_type<ext::AnyValue<vec3>>();
    ITypeRegistry::register_type<ext::AnyValue<vec4>>();
    ITypeRegistry::register_type<ext::AnyValue<size>>();
    ITypeRegistry::register_type<ext::AnyValue<rect>>();
    ITypeRegistry::register_type<ext::AnyValue<color>>();
    ITypeRegistry::register_type<ext::AnyValue<mat4>>();
    ITypeRegistry::register_type<ext::AnyValue<aabb>>();
    ITypeRegistry::register_type<ext::AnyValue<ReturnValue>>();
    ITypeRegistry::register_type<ext::AnyValue<HierarchyChange>>();

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

const TypeRegistry::Entry* TypeRegistry::find(Uid uid) const
{
    std::shared_lock lock(mutex_);
    Entry key{uid, nullptr, {}};
    auto it = std::lower_bound(types_.begin(), types_.end(), key);
    if (it != types_.end() && it->uid == uid) {
        return &*it;
    }
    return nullptr;
}

ReturnValue TypeRegistry::register_type(const IObjectFactory& factory, const TypeOptions& options)
{
    auto& info = factory.get_class_info();
    detail::velk_log(log_,
                     LogLevel::Debug,
                     __FILE__,
                     __LINE__,
                     "Register %.*s",
                     static_cast<int>(info.name.size()),
                     info.name.data());

    CreationPolicy resolved = options.policy;

    if (resolved == CreationPolicy::Auto) {
        resolved = factory.get_instance_size() < 1024 ? CreationPolicy::Hive : CreationPolicy::Alloc;
    }

    std::unique_lock lock(mutex_);
    Entry entry{info.uid, &factory, current_owner_, resolved, {}};
    auto it = std::lower_bound(types_.begin(), types_.end(), entry);
    if (it != types_.end() && it->uid == info.uid) {
        it->factory = &factory;
        it->owner = current_owner_;
        it->resolved_policy = resolved;
        it->hive.reset();
    } else {
        types_.insert(it, entry);
    }
    return ReturnValue::Success;
}

ReturnValue TypeRegistry::unregister_type(const IObjectFactory& factory)
{
    std::unique_lock lock(mutex_);
    Entry key{factory.get_class_info().uid, nullptr, {}};
    auto it = std::lower_bound(types_.begin(), types_.end(), key);
    if (it != types_.end() && it->uid == key.uid) {
#ifdef _DEBUG
        if (it->hive) {
            auto* hive = static_cast<impl::ObjectHive*>(it->hive.get());
            if (hive->has_outstanding_refs()) {
                VELK_LOG(E,
                         "unregister_type: hive for '%s' has outstanding refs",
                         it->factory->get_class_info().name.data());
            }
        }
#endif
        types_.erase(it);
    }
    return ReturnValue::Success;
}

IInterface::Ptr TypeRegistry::create(Uid uid, uint32_t flags) const
{
    const Entry* entry = find(uid);
    if (!entry) {
        VELK_LOG(E, "Failed to instantiate %s", to_string(uid).c_str());
        return {};
    }

    if (entry->resolved_policy == CreationPolicy::Hive) {
        auto* hive = ensure_hive(*entry);
        if (hive) {
            auto object = hive->allocate(flags);
            if (!object) {
                VELK_LOG(E,
                         "Failed to instantiate %s: %s",
                         entry->factory->get_class_info().name,
                         to_string(uid).c_str());
            }
            return object;
        }
    }

    auto object = entry->factory->create_instance(flags);
    if (!object) {
        VELK_LOG(
            E, "Failed to instantiate %s: %s", entry->factory->get_class_info().name, to_string(uid).c_str());
    }
    return object;
}

const ClassInfo* TypeRegistry::get_class_info(Uid classUid) const
{
    if (auto* entry = find(classUid)) {
        return &entry->factory->get_class_info();
    }
    return nullptr;
}

const IObjectFactory* TypeRegistry::find_factory(Uid classUid) const
{
    if (auto* entry = find(classUid)) {
        return entry->factory;
    }
    return nullptr;
}

void TypeRegistry::set_owner(Uid uid)
{
    std::unique_lock lock(mutex_);
    current_owner_ = uid;
}

void TypeRegistry::sweep_owner(Uid uid)
{
    std::unique_lock lock(mutex_);

    types_.erase(std::remove_if(types_.begin(), types_.end(), [&](const Entry& e) { return e.owner == uid; }),
                 types_.end());

    interpolators_.erase(std::remove_if(interpolators_.begin(),
                                        interpolators_.end(),
                                        [&](const InterpolatorEntry& e) { return e.owner == uid; }),
                         interpolators_.end());
}

size_t TypeRegistry::check_owner_hives(Uid uid) const
{
#ifdef _DEBUG
    std::shared_lock lock(mutex_);
    size_t count = 0;
    for (const auto& e : types_) {
        if (e.owner == uid && e.hive) {
            auto* hive = static_cast<impl::ObjectHive*>(e.hive.get());
            if (hive->has_outstanding_refs()) {
                VELK_LOG(E,
                         "check_owner_hives: hive for '%s' has outstanding refs",
                         e.factory->get_class_info().name.data());
                ++count;
            }
        }
    }
    return count;
#else
    (void)uid;
    return 0;
#endif
}

impl::ObjectHive* TypeRegistry::ensure_hive(const Entry& entry) const
{
    std::unique_lock lock(mutex_);
    if (entry.hive) {
        return static_cast<impl::ObjectHive*>(entry.hive.get());
    }

    auto hive_obj = ext::make_object<impl::ObjectHive>();
    auto* hive = static_cast<impl::ObjectHive*>(hive_obj.get());
    hive->init(entry.uid, *entry.factory);
    entry.hive = std::move(hive_obj);
    return hive;
}

ReturnValue TypeRegistry::register_interpolator(Uid typeUid, InterpolatorFn fn)
{
    std::unique_lock lock(mutex_);
    InterpolatorEntry entry{typeUid, fn, current_owner_};
    auto it = std::lower_bound(interpolators_.begin(), interpolators_.end(), entry);
    if (it != interpolators_.end() && it->typeUid == typeUid) {
        it->fn = fn;
        it->owner = current_owner_;
    } else {
        interpolators_.insert(it, entry);
    }
    return ReturnValue::Success;
}

ReturnValue TypeRegistry::unregister_interpolator(Uid typeUid)
{
    std::unique_lock lock(mutex_);
    InterpolatorEntry key{typeUid, nullptr, {}};
    auto it = std::lower_bound(interpolators_.begin(), interpolators_.end(), key);
    if (it != interpolators_.end() && it->typeUid == typeUid) {
        interpolators_.erase(it);
        return ReturnValue::Success;
    }
    return ReturnValue::NothingToDo;
}

InterpolatorFn TypeRegistry::find_interpolator(Uid typeUid) const
{
    std::shared_lock lock(mutex_);
    InterpolatorEntry key{typeUid, nullptr, {}};
    auto it = std::lower_bound(interpolators_.begin(), interpolators_.end(), key);
    if (it != interpolators_.end() && it->typeUid == typeUid) {
        return it->fn;
    }
    return nullptr;
}

void TypeRegistry::create_event_once(IEvent::Ptr& slot) const
{
    std::unique_lock lock(mutex_);
    if (!slot) {
        static const auto& factory = impl::Event::get_factory();
        slot = interface_pointer_cast<IEvent>(factory.create_instance());
    }
}

IAny::Ptr TypeRegistry::create_any(Uid type) const
{
    return interface_pointer_cast<IAny>(create(type));
}

IVariant::Ptr TypeRegistry::create_variant() const
{
    static const auto& factory = impl::Variant::get_factory();
    return interface_pointer_cast<IVariant>(factory.create_instance());
}

IObjectRef::Ptr TypeRegistry::create_object_ref() const
{
    static const auto& factory = impl::ObjectRef::get_factory();
    return interface_pointer_cast<IObjectRef>(factory.create_instance());
}

IFuture::Ptr TypeRegistry::create_future() const
{
    static const auto& factory = impl::Future::get_factory();
    return interface_pointer_cast<IFuture>(factory.create_instance());
}

IFunction::Ptr TypeRegistry::create_callback(IFunction::CallableFn* fn) const
{
    static const auto& factory = impl::Function::get_factory();
    auto func = interface_pointer_cast<IFunction>(factory.create_instance());
    if (fn) {
        if (auto* internal = interface_cast<IFunctionInternal>(func)) {
            internal->set_invoke_callback(fn);
        }
    }
    return func;
}

IFunction::Ptr TypeRegistry::create_owned_callback(void* context, IFunction::BoundFn* fn,
                                                   IFunction::ContextDeleter* deleter) const
{
    static const auto& factory = impl::Function::get_factory();
    auto func = interface_pointer_cast<IFunction>(factory.create_instance());
    if (fn && deleter) {
        if (auto* internal = interface_cast<IFunctionInternal>(func)) {
            internal->set_owned_callback(context, fn, deleter);
        }
    }
    return func;
}

Uid TypeRegistry::find_class_by_name(string_view name) const
{
    // Check for scoped name: "plugin_name.ClassName"
    size_t dot = name.find(".");
    if (dot != string_view::npos) {
        auto plugin_name = string_view(name.data(), dot);
        auto class_name = string_view(name.data() + dot + 1, name.size() - dot - 1);

        auto& reg = instance().plugin_registry();
        std::shared_lock lock(mutex_);
        for (const auto& entry : types_) {
            if (!entry.factory || entry.owner == Uid{}) {
                continue;
            }
            auto plugin = reg.find_plugin(entry.owner);
            if (plugin && plugin->get_plugin_info().name == plugin_name) {
                auto& info = entry.factory->get_class_info();
                if (info.name == class_name) {
                    return info.uid;
                }
            }
        }
        return {};
    }

    // Unscoped: match by class name directly
    std::shared_lock lock(mutex_);
    for (const auto& entry : types_) {
        if (entry.factory) {
            auto& info = entry.factory->get_class_info();
            if (info.name == name) {
                return info.uid;
            }
        }
    }
    return {};
}

void TypeRegistry::for_each_class(void* ctx, ClassVisitorFn visitor) const
{
    std::shared_lock lock(mutex_);
    for (const auto& entry : types_) {
        if (entry.factory) {
            if (!visitor(ctx, entry.factory->get_class_info())) {
                break;
            }
        }
    }
}

void TypeRegistry::for_each_instance(Uid classUid, void* ctx, InstanceVisitorFn visitor) const
{
    // Visit outside the lock: the visitor may create objects, which takes it again.
    IObject::Ptr hive;
    if (const Entry* entry = find(classUid)) {
        std::shared_lock lock(mutex_);
        hive = entry->hive;
    }
    // The registry's objects are born zombies, which IObjectHive::for_each skips.
    if (hive) {
        static_cast<impl::ObjectHive*>(hive.get())->for_each_live(ctx, visitor);
    }
}

IProperty::Ptr TypeRegistry::create_property(Uid type, const IAny::Ptr& value, uint32_t flags) const
{
    static const auto& factory = impl::StandaloneProperty::get_factory();
    auto property = interface_pointer_cast<IProperty>(factory.create_instance(flags));
    if (auto pi = interface_cast<IPropertyInternal>(property)) {
        if (value && is_compatible(value, type)) {
            if (pi->set_any(value)) {
                return property;
            }
            VELK_LOG(E, "Initial value is of incompatible type");
        }
        // Any was not specified for property instance, create new one
        if (auto any = create_any(type)) {
            if (pi->set_any(any)) {
                return property;
            }
        }
    }
    return {};
}

vector<TypeStats> TypeRegistry::gather_type_stats() const
{
    std::shared_lock lock(mutex_);
    vector<TypeStats> result;
    result.reserve(types_.size());
    for (const auto& entry : types_) {
        if (entry.factory) {
            TypeStats ts{};
            ts.factory = entry.factory;
            ts.owner = entry.owner;
            ts.policy = entry.resolved_policy;
            if (entry.hive) {
                auto* hive = interface_cast<IHive>(entry.hive.get());
                ts.instance_count = hive ? hive->allocated_count() : 0;
            }
            result.emplace_back(std::move(ts));
        }
    }
    return result;
}

} // namespace velk
