#ifndef INTF_VELK_H
#define INTF_VELK_H

#include <velk/duration.h>
#include <velk/interface/resource/intf_resource_store.h>
#include <velk/interface/intf_log.h>
#include <velk/interface/intf_perf_log.h>
#include <velk/interface/intf_object.h>
#include <velk/interface/intf_object_factory.h>
#include <velk/interface/intf_plugin_registry.h>
#include <velk/interface/intf_task_pool.h>
#include <velk/interface/intf_type_registry.h>
#include <velk/interface/types.h>
#include <velk/vector.h>
#include <velk/velk_export.h>

namespace velk {

class IObjectStorage;

/** @brief Owns cloned function args and lazily builds the raw pointer array for FnArgs. */
struct DeferredArgs : public ::velk::NoCopyMove
{
    /** @brief Deep-clones each argument from @p args. */
    explicit DeferredArgs(FnArgs args)
    {
        owned_.reserve(args.count);
        for (auto* arg : args) {
            owned_.push_back(arg ? arg->clone() : nullptr);
        }
    }

    /** @brief Returns a non-owning FnArgs view. Builds the raw pointer array on first call. */
    FnArgs view() const
    {
        if (owned_.size() && ptrs_.size() != owned_.size()) {
            ptrs_.resize(owned_.size());
            for (size_t i = 0; i < owned_.size(); ++i) {
                ptrs_[i] = owned_[i].get();
            }
        }
        return {ptrs_.data(), ptrs_.size()};
    }

private:
    vector<IAny::Ptr> owned_;
    mutable vector<const IAny*> ptrs_;
};

/** @brief Deferred task */
struct DeferredTask
{
    /** @brief The function to invoke. */
    IFunction::ConstPtr fn;
    /** @brief Cloned function args. Shared across tasks that originated from the same invocation. */
    shared_ptr<DeferredArgs> args;
};

/** @brief Deferred property write queued for the next update() call. */
struct DeferredPropertySet
{
    IPropertyInternal::WeakPtr property; ///< Weak ref to the property. Skipped if expired before flush.
    IAny::Ptr value;                     ///< Cloned value to apply.
};

/** @brief Information passed to each update cycle. */
struct UpdateInfo
{
    Duration time;    ///< Time since the instance was created.
    Duration elapsed; ///< Time since the first update() call.
    Duration dt;      ///< Time since the previous update() call.
};

/**
 * @brief Central interface for creating and managing Velk object types.
 *
 * Types are registered via IObjectFactory instances and can be created by UID.
 *
 * A global IVelk& reference be retrieved through velk::instance():
 * @code
 * #include <velk/api/velk.h>
 * ...
 * auto& s = ::velk::instance(); // velk::IVelk&
 * @endcode
 */
/** @brief Per-type runtime statistics. */
struct TypeStats
{
    const IObjectFactory* factory; ///< Factory for this type (ClassInfo, interfaces, members).
    Uid owner;                     ///< Plugin that registered it, empty = builtin.
    size_t instance_count;         ///< Hive size. 0 if no hive or heap-allocated.
    CreationPolicy policy;
};

/** @brief Per-plugin runtime statistics. */
struct PluginStats
{
    Uid plugin_uid;
    string_view plugin_name;
    uint32_t version;
    bool update_enabled;
};

/** @brief Snapshot of the velk instance runtime state. */
struct VelkStats
{
    vector<TypeStats> types;
    vector<PluginStats> plugins;
};

class IVelk : public Interface<IVelk>
{
public:
    /** @brief Returns the type registry for registering/querying types. */
    virtual ITypeRegistry& type_registry() = 0;
    /** @brief Returns the type registry (const). */
    virtual const ITypeRegistry& type_registry() const = 0;

    /** @brief Returns the plugin registry for loading/unloading plugins. */
    virtual IPluginRegistry& plugin_registry() = 0;
    /** @brief Returns the plugin registry (const). */
    virtual const IPluginRegistry& plugin_registry() const = 0;

    /** @brief Returns the log interface for configuring and emitting log messages. */
    virtual ILog& log() = 0;
    /** @brief Returns the log interface (const). */
    virtual const ILog& log() const = 0;

    /** @brief Returns the performance log interface for scoped timing measurements. */
    virtual IPerfLog& perf_log() = 0;
    /** @brief Returns the performance log interface (const). */
    virtual const IPerfLog& perf_log() const = 0;

    /** @brief Returns the resource store for URI-based resource access. */
    virtual IResourceStore& resource_store() = 0;
    /** @brief Returns the resource store (const). */
    virtual const IResourceStore& resource_store() const = 0;

    /** @brief Returns the shared background task pool. Worker threads start on the first submit. */
    virtual ITaskPool::Ptr task_pool() const = 0;

    /**
     * @brief Enqueues tasks to be executed on the next update() call.
     * @param tasks The tasks to invoke.
     */
    virtual void queue_deferred_tasks(array_view<DeferredTask> tasks) const = 0;
    /**
     * @brief Enqueues a deferred property write for the next update() call.
     * @param task The deferred property set to queue.
     */
    virtual void queue_deferred_property(DeferredPropertySet task) const = 0;
    /**
     * @brief Executes all queued deferred tasks and notifies opted-in plugins.
     * @param time Current time in microseconds. If zero, the system clock is used.
     */
    virtual void update(Duration time = {}) const = 0;

    /** @brief Returns a snapshot of the runtime state (registered types, plugins, instance counts). */
    virtual VelkStats get_stats() const = 0;

    /** @brief Creates an ObjectStorage for the given class info and owner. */
    virtual IObjectStorage* create_metadata_container(const ClassInfo& info, IInterface* owner) const = 0;
    /** @brief Destroys an ObjectStorage previously created by create_metadata_container. */
    virtual void destroy_metadata_container(IObjectStorage* storage) const = 0;

    /** @brief Creates an instance of a registered type by its UID. */
    IInterface::Ptr create(Uid uid, uint32_t flags = ObjectFlags::None) const
    {
        return type_registry().create(uid, flags);
    }
    /** @brief Creates a new IAny value container for the given type UID. */
    IAny::Ptr create_any(Uid type) const { return type_registry().create_any(type); }
    /** @brief Creates a new Variant value container that can hold any type. */
    IVariant::Ptr create_variant() const { return type_registry().create_variant(); }
    /** @brief Creates a new ObjectRef value container for storing object references. */
    IObjectRef::Ptr create_object_ref() const { return type_registry().create_object_ref(); }
    /** @brief Creates a new property instance with the given type and optional initial value. */
    IProperty::Ptr create_property(Uid type, const IAny::Ptr& value, uint32_t flags = ObjectFlags::None) const
    {
        return type_registry().create_property(type, value, flags);
    }
    /** @brief Creates a new future/promise pair. */
    IFuture::Ptr create_future() const { return type_registry().create_future(); }
    /** @brief Creates a callback-backed IFunction from a raw function pointer. */
    IFunction::Ptr create_callback(IFunction::CallableFn* fn) const
    {
        return type_registry().create_callback(fn);
    }
    /** @brief Creates an owned-callback IFunction from a context, trampoline, and deleter. */
    IFunction::Ptr create_owned_callback(void* context, IFunction::BoundFn* fn,
                                         IFunction::ContextDeleter* deleter) const
    {
        return type_registry().create_owned_callback(context, fn, deleter);
    }
    /**
     * @brief Creates a property for type T with an optional initial value.
     * @tparam T The value type for the property.
     */
    template <class T>
    IProperty::Ptr create_property(const IAny::Ptr& value = {}, uint32_t flags = ObjectFlags::None) const
    {
        return create_property(type_uid<T>(), value, flags);
    }
    /**
     * @brief Creates an instance and casts it to the specified interface type.
     * @tparam T The target interface type.
     */
    template <class T>
    typename T::Ptr create(Uid uid, uint32_t flags = ObjectFlags::None) const
    {
        return interface_pointer_cast<T>(create(uid, flags));
    }
};

/**
 * @brief Registers a type for a given velk instance using its static get_factory() method.
 * @tparam T An Object-derived class with a static get_factory() method.
 */
template <class T>
ReturnValue register_type(IVelk& instance, const TypeOptions& options = {})
{
    return instance.type_registry().register_type(T::get_factory(), options);
}

/**
 * @brief Unregisters a previously registered type from a given velk instance using
 *        its static get_factory() method.
 * @tparam T An Object-derived class with a static get_factory() method.
 */
template <class T>
ReturnValue unregister_type(IVelk& instance)
{
    return instance.type_registry().unregister_type(T::get_factory());
}

} // namespace velk

#endif // INTF_VELK_H
