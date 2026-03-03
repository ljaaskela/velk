#ifndef VELK_EXT_NODE_H
#define VELK_EXT_NODE_H

#include <velk/ext/object.h>
#include <velk/interface/intf_node.h>

namespace velk::detail {
class NodeBase
{
protected:
    IContainer* storage_ensure_container(IObjectStorage& storage) const
    {
        if (!container_) {
            container_ =
                interface_cast<IContainer>(storage.find_attachment(container_query_, Resolve::Create));
        }
        return container_;
    }

    IContainer* storage_find_container(IObjectStorage& storage) const
    {
        if (!container_) {
            container_ =
                interface_cast<IContainer>(storage.find_attachment(container_query_, Resolve::Existing));
        }
        return container_;
    }

    static constexpr AttachmentQuery container_query_{{}, ClassId::Container};
    mutable IContainer* container_ = nullptr;
};

} // namespace velk::detail

namespace velk::ext {

/**
 * @brief CRTP base for objects that participate in a hierarchy.
 *
 * Extends ext::Object with INode (and thus IContainer). All container
 * operations delegate to a lazily-created ClassId::Container attachment.
 * Read operations on a fresh node are free (no allocation).
 *
 * Usage:
 * @code
 *   class MyNode : public ext::Node<MyNode, IMyInterface> {};
 * @endcode
 *
 * @tparam FinalClass The final derived class (CRTP parameter).
 * @tparam Interfaces Additional interfaces the object implements.
 */
template <class FinalClass, class... Interfaces>
class Node : public Object<FinalClass, INode, Interfaces...>, protected ::velk::detail::NodeBase
{
public:
    Node() = default;
    ~Node() override = default;

public: // IContainer overrides
    ReturnValue add(const IObject::Ptr& child) override
    {
        auto* c = ensure_container();
        return c ? c->add(child) : ReturnValue::Fail;
    }

    ReturnValue remove(const IObject::Ptr& child) override
    {
        auto* c = find_container();
        return c ? c->remove(child) : ReturnValue::NothingToDo;
    }

    ReturnValue insert(size_t index, const IObject::Ptr& child) override
    {
        auto* c = ensure_container();
        return c ? c->insert(index, child) : ReturnValue::Fail;
    }

    ReturnValue replace(size_t index, const IObject::Ptr& child) override
    {
        auto* c = find_container();
        return c ? c->replace(index, child) : ReturnValue::InvalidArgument;
    }

    IObject::Ptr get_at(size_t index) const override
    {
        auto* c = find_container();
        return c ? c->get_at(index) : IObject::Ptr{};
    }

    vector<IObject::Ptr> get_all() const override
    {
        auto* c = find_container();
        return c ? c->get_all() : vector<IObject::Ptr>{};
    }

    size_t size() const override
    {
        auto* c = find_container();
        return c ? c->size() : 0;
    }

    void clear() override
    {
        if (auto* c = find_container()) {
            c->clear();
        }
    }

private:
    IContainer* ensure_container() const { return storage_ensure_container(*const_cast<Node*>(this)); }
    IContainer* find_container() const { return storage_find_container(*const_cast<Node*>(this)); }
};

} // namespace velk::ext

#endif // VELK_EXT_NODE_H
