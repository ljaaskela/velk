#ifndef INTF_VELK_H
#define INTF_VELK_H

#include <velk/interface/intf_future.h>
#include <velk/interface/intf_log.h>
#include <velk/interface/intf_object.h>
#include <velk/interface/intf_object_factory.h>
#include <velk/interface/intf_plugin_registry.h>
#include <velk/interface/intf_property.h>
#include <velk/interface/intf_type_registry.h>
#include <velk/interface/types.h>
#include <velk/velk_export.h>

#include <vector>

namespace velk {

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
    std::vector<IAny::Ptr> owned_;
    mutable std::vector<const IAny*> ptrs_;
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
    Duration timeSinceInit;        ///< Time elapsed since the instance was created.
    Duration timeSinceFirstUpdate; ///< Time elapsed since the first update() call.
    Duration timeSinceLastUpdate;  ///< Time elapsed since the previous update() call.
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

    /** @brief Creates an instance of a registered type by its UID. */
    virtual IInterface::Ptr create(Uid uid) const = 0;
    /** @brief Creates a new IAny value container for the given type UID. */
    virtual IAny::Ptr create_any(Uid type) const = 0;
    /** @brief Creates a new property instance with the given type and optional initial value. */
    virtual IProperty::Ptr create_property(Uid type, const IAny::Ptr& value,
                                           int32_t flags = ObjectFlags::None) const = 0;
    /** @brief Creates a new future/promise pair. */
    virtual IFuture::Ptr create_future() const = 0;
    /** @brief Creates a callback-backed IFunction from a raw function pointer. */
    virtual IFunction::Ptr create_callback(IFunction::CallableFn* fn) const = 0;
    /** @brief Creates an owned-callback IFunction from a context, trampoline, and deleter. */
    virtual IFunction::Ptr create_owned_callback(void* context, IFunction::BoundFn* fn,
                                                 IFunction::ContextDeleter* deleter) const = 0;
    /**
     * @brief Creates a property for type T with an optional initial value.
     * @tparam T The value type for the property.
     */
    template <class T>
    IProperty::Ptr create_property(const IAny::Ptr& value = {}, int32_t flags = ObjectFlags::None) const
    {
        return create_property(type_uid<T>(), value, flags);
    }
    /**
     * @brief Creates an instance and casts it to the specified interface type.
     * @tparam T The target interface type.
     */
    template <class T>
    typename T::Ptr create(Uid uid) const
    {
        return interface_pointer_cast<T>(create(uid));
    }
};

/**
 * @brief Registers a type for a given velk instance using its static get_factory() method.
 * @tparam T An Object-derived class with a static get_factory() method.
 */
template <class T>
ReturnValue register_type(IVelk& instance)
{
    return instance.type_registry().register_type(T::get_factory());
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
