#ifndef VELK_TYPE_REGISTRY_H
#define VELK_TYPE_REGISTRY_H

#include "hive/object_hive.h"

#include <velk/ext/interface_dispatch.h>
#include <velk/interface/intf_log.h>
#include <velk/interface/intf_type_registry.h>
#include <velk/interface/intf_velk.h>
#include <velk/vector.h>

#include <shared_mutex>

namespace velk {

/**
 * @brief Concrete implementation of ITypeRegistry.
 *
 * Maintains a sorted vector of type factories keyed by class UID.
 * Owned as a stack member by VelkInstance.
 */
class TypeRegistry final : public ext::InterfaceDispatch<ITypeRegistry>
{
public:
    explicit TypeRegistry(ILog& log);

    // ITypeRegistry overrides
    ReturnValue register_type(const IObjectFactory& factory, const TypeOptions& options = {}) override;
    ReturnValue unregister_type(const IObjectFactory& factory) override;
    const ClassInfo* get_class_info(Uid classUid) const override;
    const IObjectFactory* find_factory(Uid classUid) const override;
    ReturnValue register_interpolator(Uid typeUid, InterpolatorFn fn) override;
    ReturnValue unregister_interpolator(Uid typeUid) override;
    InterpolatorFn find_interpolator(Uid typeUid) const override;

    /** @brief Creates an instance of a registered type by its UID. */
    IInterface::Ptr create(Uid uid, uint32_t flags = ObjectFlags::None) const override;
    /** @brief Sets the current owner context for subsequent register_type calls. */
    void set_owner(Uid uid);
    /** @brief Erases all entries owned by the given plugin UID. */
    void sweep_owner(Uid uid);
    /** @brief Checks hives owned by uid for outstanding refs. Returns count. Debug only. */
    size_t check_owner_hives(Uid uid) const;

    void create_event_once(IEvent::Ptr& slot) const override;
    IAny::Ptr create_any(Uid type) const override;
    IVariant::Ptr create_variant() const override;
    IObjectRef::Ptr create_object_ref() const override;
    IProperty::Ptr create_property(Uid type, const IAny::Ptr& value, uint32_t flags) const override;
    IFuture::Ptr create_future() const override;
    IFunction::Ptr create_callback(IFunction::CallableFn* fn) const override;
    IFunction::Ptr create_owned_callback(void* context, IFunction::BoundFn* fn,
                                         IFunction::ContextDeleter* deleter) const override;
    Uid find_class_by_name(string_view name) const override;
    void for_each_class(void* ctx, ClassVisitorFn visitor) const override;
    void for_each_instance(Uid classUid, void* ctx, InstanceVisitorFn visitor) const override;

    /** @brief Gathers per-type stats for all registered types. */
    vector<TypeStats> gather_type_stats() const;

private:
    /** @brief Registry entry mapping a class UID to its factory. */
    struct Entry
    {
        Uid uid;                       ///< Class UID.
        const IObjectFactory* factory; ///< Factory that creates instances of this class.
        Uid owner;                     ///< Plugin that registered this type (Uid{} = builtin).
        CreationPolicy resolved_policy{CreationPolicy::Alloc}; ///< Resolved allocation policy.
        mutable IObject::Ptr hive;                             ///< Lazy hive (null until first use).
        bool operator<(const Entry& o) const { return uid < o.uid; }
    };

    /** @brief Finds the entry for the given class UID, or nullptr if not registered. */
    const Entry* find(Uid uid) const;

    /** @brief Lazily creates and caches a hive for the given entry. */
    impl::ObjectHive* ensure_hive(const Entry& entry) const;

    /** @brief Registry entry mapping a type UID to its interpolator function. */
    struct InterpolatorEntry
    {
        Uid typeUid;
        InterpolatorFn fn;
        Uid owner;
        bool operator<(const InterpolatorEntry& o) const { return typeUid < o.typeUid; }
    };

    vector<Entry> types_;                     ///< Sorted registry of class factories.
    vector<InterpolatorEntry> interpolators_; ///< Sorted registry of interpolator functions.
    Uid current_owner_;                       ///< Owner context for type registration.
    mutable std::shared_mutex mutex_;         ///< Protects types_, interpolators_, current_owner_.
    ILog& log_;
};

} // namespace velk

#endif // VELK_TYPE_REGISTRY_H
