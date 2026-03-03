#ifndef NODE_H
#define NODE_H

#include <velk/ext/node.h>

namespace velk {

/**
 * @brief Default built-in node type registered as ClassId::Node.
 */
class NodeImpl final : public ext::Node<NodeImpl>
{
public:
    VELK_CLASS_UID(ClassId::Node);
};

} // namespace velk

#endif // NODE_H
