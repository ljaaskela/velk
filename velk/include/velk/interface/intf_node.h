#ifndef VELK_INTF_NODE_H
#define VELK_INTF_NODE_H

#include <velk/interface/intf_container.h>

namespace velk {

/**
 * @brief Interface marker for objects that participate in a hierarchy.
 *
 * Extends IContainer with the semantic meaning of "this object is a node
 * in a tree". Implementations lazy-create a ClassId::Container attachment
 * for actual child storage on first mutation.
 *
 * Chain: IInterface -> IContainer -> INode
 */
class INode : public Interface<INode, IContainer>
{
};

} // namespace velk

#endif // VELK_INTF_NODE_H
