#ifndef INTF_STRATA_H
#define INTF_STRATA_H

#include <functional>
#include <string_view>

#include <interface/intf_object.h>
#include <interface/intf_object_factory.h>
#include <interface/intf_property.h>
#include <interface/types.h>

namespace strata {

/**
 * @brief Central interface for creating and managing Strata object types.
 *
 * Types are registered via IObjectFactory instances and can be created by UID.
 */
class IStrata : public Interface<IStrata>
{
public:
    /** @brief Factory callable that creates an IObject instance. */
    using TypeCreateFn = std::function<IObject::Ptr()>;

    /** @brief Registers an object factory for the type it describes. */
    virtual ReturnValue register_type(const IObjectFactory &factory) = 0;
    /** @brief Unregisters a previously registered object factory. */
    virtual ReturnValue unregister_type(const IObjectFactory &factory) = 0;
    /** @brief Creates an instance of a registered type by its UID. */
    virtual IInterface::Ptr create(Uid uid) const = 0;
    /** @brief Returns the ClassInfo for a registered type, or nullptr if not found. */
    virtual const ClassInfo* get_class_info(Uid classUid) const = 0;
    /** @brief Creates a new IAny value container for the given type UID. */
    virtual IAny::Ptr create_any(Uid type) const = 0;
    /** @brief Creates a new property instance with the given type and optional initial value. */
    virtual IProperty::Ptr create_property(Uid type, const IAny::Ptr& value) const = 0;
    /** @brief Deferred task */
    struct DeferredTask
    {
        /** @brief The function to invoke. */
        IFunction::ConstPtr fn;
        /** @brief Function args. Typically should be cloned from any source args. */
        IAny::Ptr args;
    };
    /**
     * @brief Enqueues tasks to be executed on the next update() call.
     * @param tasks The tasks to invoke.
     */
    virtual void queue_deferred_tasks(array_view<DeferredTask> tasks) const = 0;
    /** @brief Executes all queued deferred tasks. */
    virtual void update() const = 0;
    /**
     * @brief Creates a property for type T with an optional initial value.
     * @tparam T The value type for the property.
     */
    template<class T>
    IProperty::Ptr create_property(const IAny::Ptr& value = {}) const
    {
        return create_property(type_uid<T>(), value);
    }
    /**
     * @brief Creates an instance and casts it to the specified interface type.
     * @tparam T The target interface type.
     */
    template<class T>
    typename T::Ptr create(Uid uid) const
    {
        return interface_pointer_cast<T>(create(uid));
    }

    /**
     * @brief Registers a type using its static get_factory() method.
     * @tparam T An Object-derived class with a static get_factory() method.
     */
    template<class T>
    ReturnValue register_type()
    {
        return register_type(T::get_factory());
    }
};

} // namespace strata

#endif // INTF_STRATA_H
