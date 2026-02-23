#ifndef VELK_INTF_TYPE_REGISTRY_H
#define VELK_INTF_TYPE_REGISTRY_H

#include <velk/interface/intf_interface.h>
#include <velk/interface/intf_object_factory.h>

namespace velk {

/**
 * @brief Interface for registering, unregistering, and querying object type factories.
 *
 * Extracted from IVelk so that subsystems needing only type registration
 * can depend on ITypeRegistry without pulling in factory/deferred-task APIs.
 */
class ITypeRegistry : public Interface<ITypeRegistry>
{
public:
    /** @brief Registers an object factory for the type it describes. */
    virtual ReturnValue register_type(const IObjectFactory& factory) = 0;
    /** @brief Unregisters a previously registered object factory. */
    virtual ReturnValue unregister_type(const IObjectFactory& factory) = 0;
    /** @brief Returns the ClassInfo for a registered type, or nullptr if not found. */
    virtual const ClassInfo* get_class_info(Uid classUid) const = 0;

    /**
     * @brief Registers a type using its static get_factory() method.
     * @tparam T An Object-derived class with a static get_factory() method.
     */
    template <class T>
    ReturnValue register_type()
    {
        return register_type(T::get_factory());
    }
    /**
     * @brief Unregisters a previously registered type using its static get_factory() method.
     * @tparam T An Object-derived class with a static get_factory() method.
     */
    template <class T>
    ReturnValue unregister_type()
    {
        return unregister_type(T::get_factory());
    }
    /**
     * @brief Returns the ClassInfo for a type using its static class_id() method.
     * @tparam T An Object-derived class with a static class_id() method.
     */
    template <class T>
    const ClassInfo* get_class_info() const
    {
        return get_class_info(T::class_id());
    }
};

} // namespace velk

#endif // VELK_INTF_TYPE_REGISTRY_H
