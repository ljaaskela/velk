#ifndef VELK_API_CONTAINER_H
#define VELK_API_CONTAINER_H

#include <velk/api/object.h>
#include <velk/interface/intf_container.h>

#include <type_traits>

namespace velk {

/**
 * @brief Returns the IContainer attachment on an object, or nullptr if none exists.
 * @param obj The object to search for a container attachment.
 */
inline IContainer* get_container(const IInterface* obj)
{
    auto* storage = interface_cast<IObjectStorage>(obj);
    if (!storage) {
        return nullptr;
    }
    return interface_cast<IContainer>(storage->find_attachment<IContainer>());
}

/**
 * @brief Returns the existing IContainer attachment, or creates and attaches a new one.
 * @param obj The object to get or create a container on.
 * @return Shared pointer to the IContainer, or nullptr if the object has no IObjectStorage.
 */
inline IContainer::Ptr ensure_container(IInterface* obj)
{
    return find_or_create_attachment<IContainer>(obj, ClassId::Container);
}

/**
 * @brief Typed convenience wrapper around IContainer.
 *
 * Provides null-safe access and typed child operations on top of the raw
 * IContainer interface.
 *
 * The template parameter T controls the interface type used by add(),
 * remove(), get_at(), get_all(), and for_each():
 *
 *   Container<IMyWidget> c(ensure_container(obj));
 *   c.add(widget);                          // accepts IMyWidget::Ptr
 *   IMyWidget::Ptr w = c.get_at(0);         // returns IMyWidget::Ptr
 *   c.for_each([](IMyWidget& w) { ... });
 *
 * Use Container<> (defaults to IObject) when you don't need a specific interface.
 *
 * @tparam T The interface type for add/remove/get_at/get_all/for_each. Defaults to IObject.
 */
template <class T = IObject>
class Container : public Object
{
public:
    /** @brief Default-constructed Container wraps no object. */
    Container() = default;

    /** @brief Wraps an existing IObject pointer as a container. */
    explicit Container(IObject::Ptr obj) : Object(std::move(obj)) {}

    /** @brief Wraps an existing IContainer pointer. */
    explicit Container(IContainer::Ptr c)
        : Object(c ? interface_pointer_cast<IObject>(c) : IObject::Ptr{}),
          container_(c.get())
    {}

    /** @brief Returns the number of children. */
    size_t size() const
    {
        auto* c = intf();
        return c ? c->size() : 0;
    }

    /** @brief Returns true if the container is empty. */
    bool empty() const
    {
        auto* c = intf();
        return !c || c->size() == 0;
    }

    /** @brief Removes all children. */
    void clear()
    {
        if (auto* c = intf()) {
            c->clear();
        }
    }

    /** @brief Appends a child to the end of the container (null-safe). */
    ReturnValue add(const typename T::Ptr& child)
    {
        auto* c = intf();
        return c ? c->add(interface_pointer_cast<IObject>(child)) : ReturnValue::InvalidArgument;
    }

    /** @brief Removes a child by pointer identity (null-safe). */
    ReturnValue remove(const typename T::Ptr& child)
    {
        auto* c = intf();
        return c ? c->remove(interface_pointer_cast<IObject>(child)) : ReturnValue::InvalidArgument;
    }

    /** @brief Inserts a child at the given index (null-safe). */
    ReturnValue insert(size_t index, const typename T::Ptr& child)
    {
        auto* c = intf();
        return c ? c->insert(index, interface_pointer_cast<IObject>(child)) : ReturnValue::InvalidArgument;
    }

    /** @brief Replaces the child at the given index (null-safe). */
    ReturnValue replace(size_t index, const typename T::Ptr& child)
    {
        auto* c = intf();
        return c ? c->replace(index, interface_pointer_cast<IObject>(child)) : ReturnValue::InvalidArgument;
    }

    /** @brief Returns the child at the given index cast to T, or nullptr (null-safe). */
    typename T::Ptr get_at(size_t index) const
    {
        auto* c = intf();
        return c ? interface_pointer_cast<T>(c->get_at(index)) : typename T::Ptr{};
    }

    /** @brief Returns all children cast to T. */
    vector<typename T::Ptr> get_all() const
    {
        vector<typename T::Ptr> result;
        auto* c = intf();
        if (!c) {
            return result;
        }
        auto all = c->get_all();
        result.reserve(all.size());
        for (auto& child : all) {
            result.push_back(interface_pointer_cast<T>(child));
        }
        return result;
    }

    /**
     * @brief Iterates all children with a typed callback (null-safe).
     *
     * Each child is cast to T& via interface_cast. Children that do not
     * implement T are skipped.
     *
     * @param fn Callable as void(T&) or bool(T&). Return false to stop early.
     */
    template <class Fn>
    void for_each(Fn&& fn) const
    {
        static_assert(std::is_invocable_v<std::decay_t<Fn>, T&>,
                      "Container::for_each visitor must be callable as void(T&) or bool(T&)");
        auto* c = intf();
        if (!c) {
            return;
        }
        for (size_t i = 0, n = c->size(); i < n; ++i) {
            auto child = c->get_at(i);
            if (auto* typed = interface_cast<T>(child)) {
                if constexpr (std::is_same_v<decltype(fn(*typed)), bool>) {
                    if (!fn(*typed)) {
                        return;
                    }
                } else {
                    fn(*typed);
                }
            }
        }
    }

    /** @brief Returns the underlying IContainer raw pointer, or nullptr. */
    IContainer::Ptr get_container_interface() const { return as_ptr<IContainer>(); }

private:
    IContainer* intf() const
    {
        if (!container_ && get()) {
            container_ = interface_cast<IContainer>(get());
        }
        return container_;
    }

    mutable IContainer* container_ = nullptr;
};

} // namespace velk

#endif // VELK_API_CONTAINER_H
