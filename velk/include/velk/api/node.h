#ifndef VELK_API_NODE_H
#define VELK_API_NODE_H

#include <velk/api/container.h>
#include <velk/interface/intf_node.h>

namespace velk {

/**
 * @brief Convenience wrapper around any object implementing INode.
 *
 * Inherits all container operations from Container. The constructor
 * rejects objects that do not implement INode.
 */
class Node : public Container
{
public:
    Node() = default;

    explicit Node(IObject::Ptr obj)
        : Container(obj && interface_cast<INode>(obj) ? std::move(obj) : IObject::Ptr{})
    {}

    /** @brief Returns the underlying INode raw pointer, or nullptr. */
    INode::Ptr get_node_interface() const { return as_ptr<INode>(); }
};

/** @brief Creates a new ClassId::Node instance. */
inline Node create_node()
{
    return Node(instance().create<IObject>(ClassId::Node));
}

} // namespace velk

#endif // VELK_API_NODE_H
