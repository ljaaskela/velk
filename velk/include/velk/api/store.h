#ifndef VELK_API_STORE_H
#define VELK_API_STORE_H

#include <velk/api/object.h>
#include <velk/interface/intf_store.h>

namespace velk {

/**
 * @brief Convenience wrapper around IStore.
 *
 * Provides null-safe access to store operations. All methods
 * return safe defaults when the underlying object is null.
 *
 *   auto store = create_store();
 *   store.add("panel_a1", obj);
 *   auto found = store.find("panel_a1");
 */
class Store : public Object
{
public:
    /** @brief Default-constructed Store wraps no object. */
    Store() = default;

    /** @brief Wraps an existing IObject pointer, rejected if it does not implement IStore. */
    explicit Store(IObject::Ptr obj)
        : Object(obj && interface_cast<IStore>(obj) ? std::move(obj) : IObject::Ptr{})
    {}

    /** @brief Wraps an existing IStore pointer. */
    explicit Store(IStore::Ptr s)
        : Object(s ? interface_pointer_cast<IObject>(s) : IObject::Ptr{})
    {}

    /** @brief Returns an object by its string id, or nullptr. */
    IObject::Ptr find(string_view id) const
    {
        return with<IStore>([&](auto& s) { return s.find(id); });
    }

    /** @brief Returns the number of objects in the store. */
    size_t object_count() const
    {
        return with<IStore>([](auto& s) { return s.object_count(); });
    }

    /** @brief Returns the object at the given index, or nullptr. */
    IObject::Ptr object_at(size_t index) const
    {
        return with<IStore>([&](auto& s) { return s.object_at(index); });
    }

    /** @brief Returns the id of the object at the given index, or an empty string. */
    string id_at(size_t index) const
    {
        return with<IStore>([&](auto& s) { return s.id_at(index); });
    }

    /** @brief Adds an object with the given id. */
    ReturnValue add(string_view id, const IObject::Ptr& object)
    {
        return with_or<IStore>([&](auto& s) { return s.add(id, object); }, ReturnValue::InvalidArgument);
    }

    /** @brief Returns true if the store is empty. */
    bool empty() const { return object_count() == 0; }

    /** @brief Implicit conversion to IStore::Ptr. */
    operator IStore::Ptr() const { return as_ptr<IStore>(); }
};

/** @brief Creates a new Store instance. */
inline Store create_store()
{
    return Store(instance().create<IStore>(ClassId::Store));
}

} // namespace velk

#endif // VELK_API_STORE_H
