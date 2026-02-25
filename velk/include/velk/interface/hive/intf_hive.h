#ifndef VELK_INTF_HIVE_H
#define VELK_INTF_HIVE_H

#include <velk/interface/intf_object.h>

namespace velk {

namespace ClassId {
/** @brief Dense, typed container of objects sharing the same class ID. */
inline constexpr Uid Hive{"331d944c-be7d-4bb4-b5cf-91d34c1383b9"};
} // namespace ClassId

/**
 * @brief Interface for a dense, typed container of objects sharing the same class ID.
 *
 * A hive stores objects of a single class contiguously, providing cache-friendly
 * iteration and slot reuse for removed elements. Objects are full Velk objects
 * with reference counting, so they remain alive after removal as long as external
 * references exist.
 *
 * Inherits IObject so that each hive is itself a first-class Velk object.
 */
class IHive : public Interface<IHive, IObject>
{
public:
    /** @brief Returns the class UID of the objects stored in this hive. */
    virtual Uid get_element_class_uid() const = 0;

    /** @brief Returns the number of live objects in the hive. */
    virtual size_t size() const = 0;

    /** @brief Returns true if the hive contains no objects. */
    virtual bool empty() const = 0;

    /** @brief Creates a new object in the hive and returns a shared pointer to it. */
    virtual IObject::Ptr add() = 0;

    /** @brief Removes an object from the hive. Returns Success or Fail if not found. */
    virtual ReturnValue remove(IObject& object) = 0;

    /** @brief Returns true if the given object is in this hive. */
    virtual bool contains(const IObject& object) const = 0;

    /**
     * @brief Iterates all live objects in the hive.
     * @param context Opaque pointer forwarded to the visitor.
     * @param visitor Called for each live object. Return false to stop early.
     */
    using VisitorFn = bool (*)(void* context, IObject& object);
    virtual void for_each(void* context, VisitorFn visitor) const = 0;
};

} // namespace velk

#endif // VELK_INTF_HIVE_H
