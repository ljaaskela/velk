#ifndef VELK_INTF_TYPE_REGISTRY_H
#define VELK_INTF_TYPE_REGISTRY_H

#include <velk/interface/intf_any.h>
#include <velk/interface/intf_object_factory.h>
#include <velk/interface/types.h>

namespace velk {

/** @brief Interpolation callback: interpolates between two type-erased values. */
using InterpolatorFn = ReturnValue (*)(const IAny& from, const IAny& to, float t, IAny& result);

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
    /** @brief Returns the factory for a registered type, or nullptr if not found. */
    virtual const IObjectFactory* find_factory(Uid classUid) const = 0;

    /** @brief Registers an interpolator function for a given type UID. */
    virtual ReturnValue register_interpolator(Uid typeUid, InterpolatorFn fn) = 0;
    /** @brief Unregisters a previously registered interpolator for a given type UID. */
    virtual ReturnValue unregister_interpolator(Uid typeUid) = 0;
    /** @brief Finds the interpolator function for a given type UID, or nullptr if not registered. */
    virtual InterpolatorFn find_interpolator(Uid typeUid) const = 0;

    /**
     * @brief Registers an interpolator function for type T.
     * @tparam T The value type to associate the interpolator with.
     */
    template <class T>
    ReturnValue register_interpolator(InterpolatorFn fn)
    {
        return register_interpolator(type_uid<T>(), fn);
    }
    /**
     * @brief Unregisters a previously registered interpolator for type T.
     * @tparam T The value type whose interpolator should be removed.
     */
    template <class T>
    ReturnValue unregister_interpolator()
    {
        return unregister_interpolator(type_uid<T>());
    }

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
