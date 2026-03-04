#ifndef VELK_API_HIERARCHY_H
#define VELK_API_HIERARCHY_H

#include <velk/api/object.h>
#include <velk/interface/intf_hierarchy.h>

#include <type_traits>

namespace velk {

/**
 * @brief Wrapper around a HierarchyNode handle.
 *
 * Inherits Object (wrapping the node's object pointer), so all IObject
 * accessors (properties, state, attachments) work directly on the node.
 * Parent and children are queried from the hierarchy on demand.
 *
 *   auto node = h.root();
 *   node.get_children();           // vector<Node>
 *   node.get_parent();             // Node
 *   node.get_property("width");    // inherited from Object
 */
class Node : public Object
{
public:
    /** @brief Default-constructed Node wraps nothing. */
    Node() = default;

    /** @brief Wraps a HierarchyNode handle. */
    explicit Node(HierarchyNode hn)
        : Object(hn.object),
          node_(std::move(hn))
    {}

    /** @brief Returns true if this node has an object. */
    operator bool() const { return Object::operator bool(); }

    /** @brief Returns the underlying IObject pointer. */
    IObject::Ptr object() const { return node_.object; }

    /** @brief Implicit conversion to IObject::Ptr for use with Hierarchy methods. */
    operator IObject::Ptr() const { return node_.object; }

    /** @brief Returns the parent as a Node, or empty for root or if hierarchy expired. */
    Node get_parent() const
    {
        auto h = hierarchy();
        auto p = h ? h->parent_of(node_.object) : nullptr;
        return p ? Node({p, node_.hierarchy}) : Node{};
    }

    /** @brief Returns true if this node has a parent (is not root). */
    bool has_parent() const { return get_parent().operator bool(); }

    /** @brief Returns the ordered list of children as Nodes. */
    vector<Node> get_children() const
    {
        auto h = hierarchy();
        if (!h) {
            return {};
        }
        auto all = h->children_of(node_.object);
        vector<Node> result;
        result.reserve(all.size());
        for (auto& child : all) {
            result.push_back(Node({child, node_.hierarchy}));
        }
        return result;
    }

    /** @brief Returns the number of children. */
    size_t child_count() const
    {
        auto h = hierarchy();
        return h ? h->child_count(node_.object) : 0;
    }

    /** @brief Returns the child at the given index as a Node, or empty. */
    Node child_at(size_t index) const
    {
        auto h = hierarchy();
        if (!h) {
            return {};
        }
        auto c = h->child_at(node_.object, index);
        return c ? Node({c, node_.hierarchy}) : Node{};
    }

    /** @brief Returns the child at the given index cast to T, or null. */
    template <class T>
    typename T::Ptr child_at(size_t index) const
    {
        auto n = child_at(index);
        return n ? interface_pointer_cast<T>(n.object()) : typename T::Ptr{};
    }

    /**
     * @brief Iterates children with a typed callback.
     *
     * Each child is cast to T& via interface_cast. Children that do not
     * implement T are skipped.
     *
     * @tparam T The interface type to cast each child to.
     * @param fn Callable as void(T&) or bool(T&). Return false to stop early.
     */
    template <class T, class Fn>
    void for_each_child(Fn&& fn) const
    {
        static_assert(std::is_invocable_v<std::decay_t<Fn>, T&>,
                      "Node::for_each_child visitor must be callable as void(T&) or bool(T&)");
        auto h = hierarchy();
        if (!h) {
            return;
        }
        auto all = h->children_of(node_.object);
        for (auto& child : all) {
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

    /** @brief Returns the owning hierarchy, or null if expired. */
    IHierarchy::Ptr hierarchy() const { return node_.hierarchy.lock(); }

    /** @brief Returns the raw HierarchyNode handle. */
    const HierarchyNode& hierarchy_node() const { return node_; }

    /** @brief Equality: compares underlying object pointers. */
    friend bool operator==(const Node& a, const Node& b) { return a.node_.object == b.node_.object; }
    friend bool operator!=(const Node& a, const Node& b) { return !(a == b); }
    friend bool operator==(const Node& a, const IObject::Ptr& b) { return a.node_.object == b; }
    friend bool operator!=(const Node& a, const IObject::Ptr& b) { return !(a == b); }
    friend bool operator==(const IObject::Ptr& a, const Node& b) { return a == b.node_.object; }
    friend bool operator!=(const IObject::Ptr& a, const Node& b) { return !(a == b); }

private:
    HierarchyNode node_;
};

/**
 * @brief Convenience wrapper around IHierarchy.
 *
 * Provides null-safe access to hierarchy operations. All methods
 * return safe defaults when the underlying object is null.
 *
 *   auto h = create_hierarchy();
 *   h.set_root(root);
 *   h.add(root, child);
 *   auto node = h.root();
 *   node.get_children();
 */
class Hierarchy : public Object
{
public:
    /** @brief Default-constructed Hierarchy wraps no object. */
    Hierarchy() = default;

    /** @brief Wraps an existing IObject pointer, rejected if it does not implement IHierarchy. */
    explicit Hierarchy(IObject::Ptr obj)
        : Object(obj && interface_cast<IHierarchy>(obj) ? std::move(obj) : IObject::Ptr{})
    {}

    /** @brief Wraps an existing IHierarchy pointer. */
    explicit Hierarchy(IHierarchy::Ptr h)
        : Object(h ? interface_pointer_cast<IObject>(h) : IObject::Ptr{}),
          hierarchy_(h.get())
    {}

    /** @brief Sets the root object. */
    ReturnValue set_root(const IObject::Ptr& root)
    {
        auto* h = intf();
        return h ? h->set_root(root) : ReturnValue::InvalidArgument;
    }

    /** @brief Appends a child to the given parent. */
    ReturnValue add(const IObject::Ptr& parent, const IObject::Ptr& child)
    {
        auto* h = intf();
        return h ? h->add(parent, child) : ReturnValue::InvalidArgument;
    }

    /** @brief Inserts a child at the given index under the parent. */
    ReturnValue insert(const IObject::Ptr& parent, size_t index, const IObject::Ptr& child)
    {
        auto* h = intf();
        return h ? h->insert(parent, index, child) : ReturnValue::InvalidArgument;
    }

    /** @brief Removes an object and all its descendants. */
    ReturnValue remove(const IObject::Ptr& object)
    {
        auto* h = intf();
        return h ? h->remove(object) : ReturnValue::InvalidArgument;
    }

    /** @brief Replaces an object in-place, preserving position and reparenting children. */
    ReturnValue replace(const IObject::Ptr& old_child, const IObject::Ptr& new_child)
    {
        auto* h = intf();
        return h ? h->replace(old_child, new_child) : ReturnValue::InvalidArgument;
    }

    /** @brief Removes all objects from the hierarchy. */
    void clear()
    {
        if (auto* h = intf()) {
            h->clear();
        }
    }

    /** @brief Returns the root as a Node, or empty if no root is set. */
    Node root() const
    {
        auto* h = intf();
        if (!h) {
            return {};
        }
        auto r = h->root();
        return r ? Node(h->node_of(r)) : Node{};
    }

    /** @brief Returns a Node for the given object. */
    Node node_of(const IObject::Ptr& object) const
    {
        auto* h = intf();
        return h ? Node(h->node_of(object)) : Node{};
    }

    /** @brief Returns the parent as a Node, or empty if root or not found. */
    Node parent_of(const IObject::Ptr& object) const
    {
        auto* h = intf();
        if (!h) {
            return {};
        }
        auto p = h->parent_of(object);
        return p ? Node(h->node_of(p)) : Node{};
    }

    /** @brief Returns the children as Nodes. */
    vector<Node> children_of(const IObject::Ptr& object) const
    {
        auto* h = intf();
        if (!h) {
            return {};
        }
        auto all = h->children_of(object);
        vector<Node> result;
        result.reserve(all.size());
        for (auto& child : all) {
            result.push_back(Node(h->node_of(child)));
        }
        return result;
    }

    /** @brief Returns the child at the given index as a Node, or empty. */
    Node child_at(const IObject::Ptr& object, size_t index) const
    {
        auto* h = intf();
        if (!h) {
            return {};
        }
        auto c = h->child_at(object, index);
        return c ? Node(h->node_of(c)) : Node{};
    }

    /** @brief Iterates children of the given object with a typed callback. */
    template <class T, class Fn>
    void for_each_child(const IObject::Ptr& object, Fn&& fn) const
    {
        static_assert(std::is_invocable_v<std::decay_t<Fn>, T&>,
                      "Hierarchy::for_each_child visitor must be callable as void(T&) or bool(T&)");
        auto* h = intf();
        if (!h) {
            return;
        }
        auto all = h->children_of(object);
        for (auto& child : all) {
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

    /** @brief Returns the number of children of the given object. */
    size_t child_count(const IObject::Ptr& object) const
    {
        auto* h = intf();
        return h ? h->child_count(object) : 0;
    }

    /** @brief Returns true if the object is in this hierarchy. */
    bool contains(const IObject::Ptr& object) const
    {
        auto* h = intf();
        return h ? h->contains(object) : false;
    }

    /** @brief Returns the total number of objects in this hierarchy. */
    size_t size() const
    {
        auto* h = intf();
        return h ? h->size() : 0;
    }

    /** @brief Returns true if the hierarchy is empty. */
    bool empty() const
    {
        return size() == 0;
    }

    /** @brief Returns the underlying IHierarchy pointer. */
    IHierarchy::Ptr get_hierarchy_interface() const { return as_ptr<IHierarchy>(); }

private:
    IHierarchy* intf() const
    {
        if (!hierarchy_ && get()) {
            hierarchy_ = interface_cast<IHierarchy>(get());
        }
        return hierarchy_;
    }

    mutable IHierarchy* hierarchy_ = nullptr;
};

/** @brief Creates a new Hierarchy instance. */
inline Hierarchy create_hierarchy()
{
    return Hierarchy(instance().create<IHierarchy>(ClassId::Hierarchy));
}

} // namespace velk

#endif // VELK_API_HIERARCHY_H
