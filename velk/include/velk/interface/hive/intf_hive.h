#ifndef VELK_INTF_HIVE_H
#define VELK_INTF_HIVE_H

#include <velk/interface/intf_object.h>

#include <cstddef>

namespace velk {

namespace ClassId {
/** @brief Dense, typed container of objects sharing the same class ID. */
inline constexpr Uid ObjectHive{"331d944c-be7d-4bb4-b5cf-91d34c1383b9"};
/** @brief Dense, typed container for raw (non-ref-counted) allocations. */
inline constexpr Uid RawHive{"a7e1c3f0-5b29-4d8a-9f1e-3c7d2a8b4e60"};
} // namespace ClassId

/**
 * @brief The HivePageCapacity struct can be used to configure the allocation policy for each page in a hive.
 */
struct HivePageCapacity
{
    /**
     *  @brief Number of elements on page 1.
     *  @default 16
     */
    size_t page_1{16u};
    /**
     *  @brief Number of elements on page 2.
     *  @default 64
     */
    size_t page_2{64u};
    /**
     *  @brief Number of elements on page 3.
     *  @default 256
     */
    size_t page_3{256u};
    /**
     *  @brief Number of elements on page 4+.
     *  @default 1024
     */
    size_t page_n{1024u};
};

/** @brief Specifies the type of a hive. */
enum class HiveType : uint8_t
{
    ObjectHive = 0, ///< The hive stores IObject instances and implements IObjectHive.
    RawHive = 1,    ///< The hive is contains non-refcounted (usually POD) types.
};

/**
 * @brief Common base interface for all hive types.
 *
 * Provides element UID, size, and empty queries shared by both
 * object hives (ref-counted Velk objects) and raw hives (plain data).
 */
class IHive : public Interface<IHive>
{
public:
    /** @brief Returns the type of the hive. */
    virtual HiveType get_hive_type() const = 0;

    /** @brief Returns the UID identifying the element type stored in this hive. */
    virtual Uid get_element_uid() const = 0;

    /** @brief Returns the number of live elements in the hive. */
    virtual size_t size() const = 0;

    /** @brief Returns true if the hive contains no elements. */
    virtual bool empty() const = 0;

    /** @brief Returns the current page allocation policy. */
    virtual HivePageCapacity get_page_capacity() const = 0;

    /**
     *  @brief Sets the allocation policy for new pages. Requires that each page as at least one element and
     *         that at least as many items as the previous page.
     *  @note Any changes will only affect new pages, it will have no effect on existing allocations.
     *        Typically this function should be called immediately after creating a hive, before instantiating
     *        any objects to it.
     *  @param capacity The page allocation policy to set.
     */
    virtual void set_page_capacity(const HivePageCapacity& capacity) = 0;

    /**
     * @brief Removes all elements from the hive.
     *
     * For object hives, releases the hive's strong reference on every active
     * object. Objects with external references enter zombie state.
     *
     * For raw hives, reclaims all slots without calling destructors. Use
     * IRawHive::clear(context, destroy) when elements need destruction.
     *
     * After clear(), for_each() visits no elements and size() returns 0.
     */
    virtual void clear() = 0;
};

/**
 * @brief Interface for a dense, typed container of objects sharing the same class ID.
 *
 * A hive stores objects of a single class contiguously, providing cache-friendly
 * iteration and slot reuse for removed elements. Objects are full Velk objects
 * with reference counting, so they remain alive after removal as long as external
 * references exist.
 */
class IObjectHive : public Interface<IObjectHive, IHive>
{
public:
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

    /**
     * @brief Iterates all live objects, passing a pre-computed state pointer.
     *
     * All objects in a hive share the same class layout, so the byte offset
     * from object start to a given interface's State struct is constant. By
     * computing the offset once and passing it here, the visitor receives a
     * direct state pointer via pointer arithmetic, avoiding per-element
     * interface_cast and virtual get_property_state() calls. This is
     * useful for tight loops where the visitor body is cheap relative to
     * the virtual dispatch overhead (bulk reads, writes, physics ticks).
     *
     * Prefer the typed ObjectHive::for_each<StateInterface> wrapper in
     * api/hive/object_hive.h, which computes the offset automatically.
     * Use this method directly when you need to cache the offset across
     * repeated iterations or pass through a C-style context pointer.
     *
     * @param state_offset Byte offset from object start to the state struct.
     * @param context Opaque pointer forwarded to the visitor.
     * @param visitor Called with (context, object, state_ptr). Return false to stop early.
     */
    using StateVisitorFn = bool (*)(void* context, IObject& object, void* state);
    virtual void for_each_state(ptrdiff_t state_offset, void* context, StateVisitorFn visitor) const = 0;
};

/**
 * @brief Interface for a dense allocator of raw (non-ref-counted) elements.
 *
 * Provides type-erased slot allocation and deallocation without construction
 * or destruction. Callers are responsible for placement-new and explicit
 * destructor calls. Use RawHive<T> for a typed wrapper that handles
 * construction and destruction automatically.
 */
class IRawHive : public Interface<IRawHive, IHive>
{
public:
    using IHive::clear;

    /** @brief Visitor callback for raw hive iteration. Return false to stop early. */
    using RawVisitorFn = bool (*)(void* context, void* element);

    /** @brief Allocates a slot and returns a pointer to uninitialized memory. */
    virtual void* allocate() = 0;

    /** @brief Deallocates a slot. The caller must have already destroyed the object. */
    virtual void deallocate(void* ptr) = 0;

    /** @brief Returns true if the given pointer belongs to this hive and is active. */
    virtual bool contains(const void* ptr) const = 0;

    /**
     * @brief Iterates all active slots.
     * @param context Opaque pointer forwarded to the visitor.
     * @param visitor Called for each active slot. Return false to stop early.
     */
    virtual void for_each(void* context, RawVisitorFn visitor) const = 0;

    /** @brief Callback for per-element cleanup during clear(). */
    using DestroyFn = void (*)(void* context, void* element);

    /**
     * @brief Destroys all live elements and resets the hive to empty.
     *
     * Acquires an exclusive lock, calls @p destroy for each live element,
     * then reclaims all slots. Pass nullptr for @p destroy to skip
     * element destruction (e.g. for trivially destructible types).
     *
     * @param context Opaque pointer forwarded to the destroy callback.
     * @param destroy Called for each live element before its slot is reclaimed.
     */
    virtual void clear(void* context, DestroyFn destroy) = 0;
};

} // namespace velk

#endif // VELK_INTF_HIVE_H
