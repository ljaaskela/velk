#ifndef VELK_API_CONTAINER_H
#define VELK_API_CONTAINER_H

#include <velk/api/object.h>
#include <velk/interface/intf_container.h>

#include <type_traits>

namespace velk {

/**
 * @brief Convenience wrapper around IContainer.
 *
 * Provides null-safe access and child operations on top of the raw
 * IContainer interface. Templated methods provide typed access where needed:
 *
 *   Container c(create_container());
 *   c.add(child);
 *   IMyWidget::Ptr w = c.get_at<IMyWidget>(0);
 *   c.for_each<IMyWidget>([](IMyWidget& w) { ... });
 */
class Container : public Object
{
public:
    /** @brief Default-constructed Container wraps no object. */
    Container() = default;

    /** @brief Wraps an existing IObject pointer, rejected if it does not implement IContainer. */
    explicit Container(IObject::Ptr obj)
        : Object(obj && interface_cast<IContainer>(obj) ? std::move(obj) : IObject::Ptr{})
    {}

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

    /** @brief Appends a child to the end of the container. */
    ReturnValue add(const IObject::Ptr& child)
    {
        auto* c = intf();
        return c ? c->add(child) : ReturnValue::InvalidArgument;
    }

    /** @brief Removes a child by pointer identity. */
    ReturnValue remove(const IObject::Ptr& child)
    {
        auto* c = intf();
        return c ? c->remove(child) : ReturnValue::InvalidArgument;
    }

    /** @brief Inserts a child at the given index. */
    ReturnValue insert(size_t index, const IObject::Ptr& child)
    {
        auto* c = intf();
        return c ? c->insert(index, child) : ReturnValue::InvalidArgument;
    }

    /** @brief Replaces the child at the given index. */
    ReturnValue replace(size_t index, const IObject::Ptr& child)
    {
        auto* c = intf();
        return c ? c->replace(index, child) : ReturnValue::InvalidArgument;
    }

    /** @brief Returns the child at the given index, or nullptr. */
    IObject::Ptr get_at(size_t index) const
    {
        auto* c = intf();
        return c ? c->get_at(index) : IObject::Ptr{};
    }

    /** @brief Returns the child at the given index cast to T, or nullptr. */
    template <class T>
    typename T::Ptr get_at(size_t index) const
    {
        auto* c = intf();
        return c ? interface_pointer_cast<T>(c->get_at(index)) : typename T::Ptr{};
    }

    /** @brief Returns all children. */
    vector<IObject::Ptr> get_all() const
    {
        auto* c = intf();
        return c ? c->get_all() : vector<IObject::Ptr>{};
    }

    /** @brief Returns all children cast to T. */
    template <class T>
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
     * @brief Iterates all children with a typed callback.
     *
     * Each child is cast to T& via interface_cast. Children that do not
     * implement T are skipped.
     *
     * @tparam T The interface type to cast each child to.
     * @param fn Callable as void(T&) or bool(T&). Return false to stop early.
     */
    template <class T, class Fn>
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

/** @brief Creates a new ClassId::Container instance. */
inline Container create_container()
{
    return Container(instance().create<IContainer>(ClassId::Container));
}

} // namespace velk

#endif // VELK_API_CONTAINER_H
