#ifndef VELK_INTF_OBJECT_FACTORY_H
#define VELK_INTF_OBJECT_FACTORY_H

#include <velk/interface/intf_object.h>
#include <velk/interface/types.h>

namespace velk {

/** @brief Interface for factories that create IObject instances of a specific class. */
class IObjectFactory : public Interface<IObjectFactory>
{
public:
    /** @brief Creates a new instance of the class this factory represents. */
    virtual IObject::Ptr create_instance(uint32_t flags = ObjectFlags::None) const = 0;
    /** @brief Returns the ClassInfo describing the class this factory creates. */
    virtual const ClassInfo& get_class_info() const = 0;

    /** @brief Returns the size (in bytes) of one instance. */
    virtual size_t get_instance_size() const = 0;
    /** @brief Returns the alignment requirement (in bytes) of one instance. */
    virtual size_t get_instance_alignment() const = 0;
    /**
     * @brief Placement-constructs a default instance at the given location.
     * @param location Aligned memory for the object.
     * @param block If non-null, installs this control block on the object and
     *              returns the auto-allocated block to the pool.
     * @return The IObject* for the constructed object.
     */
    virtual IObject* construct_in_place(void* location, control_block* block = nullptr,
                                        uint32_t flags = ObjectFlags::None) const = 0;
    /** @brief Calls the destructor of an instance at the given location (does not free memory). */
    virtual void destroy_in_place(void* location) const = 0;

    /** @brief Type helper create_instance(). */
    template <class T>
    typename T::Ptr create_instance(uint32_t flags = ObjectFlags::None) const
    {
        return interface_pointer_cast<T>(create_instance(flags));
    }
};

} // namespace velk

#endif // VELK_INTF_OBJECT_FACTORY_H
