#include <velk/api/hierarchy.h>
#include <velk/api/velk.h>
#include <velk/ext/object.h>
#include <velk/interface/intf_hierarchy.h>

#include <gtest/gtest.h>

using namespace velk;

class IHierarchyTest : public Interface<IHierarchyTest>
{
public:
    VELK_INTERFACE(
        (PROP, int, value, 0)
    )
};

class HierarchyTestObj : public ext::Object<HierarchyTestObj, IHierarchyTest>
{};

class HierarchyTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        instance().type_registry().register_type<HierarchyTestObj>();
    }

    IObject::Ptr make_obj()
    {
        return instance().create<IObject>(HierarchyTestObj::class_id());
    }
};

TEST_F(HierarchyTest, CreateHierarchy)
{
    auto h = create_hierarchy();
    EXPECT_TRUE(h);
    EXPECT_EQ(h.size(), 0u);
    EXPECT_TRUE(h.empty());
}

TEST_F(HierarchyTest, SetRoot)
{
    auto h = create_hierarchy();
    auto root = make_obj();

    EXPECT_TRUE(succeeded(h.set_root(root)));
    EXPECT_EQ(h.root(), root);
    EXPECT_EQ(h.size(), 1u);
    EXPECT_FALSE(h.empty());
}

TEST_F(HierarchyTest, SetRootNullFails)
{
    auto h = create_hierarchy();
    EXPECT_EQ(h.set_root({}), ReturnValue::InvalidArgument);
}

TEST_F(HierarchyTest, SetRootClearsExisting)
{
    auto h = create_hierarchy();
    auto root1 = make_obj();
    auto root2 = make_obj();
    auto child = make_obj();

    h.set_root(root1);
    h.add(root1, child);
    EXPECT_EQ(h.size(), 2u);

    h.set_root(root2);
    EXPECT_EQ(h.size(), 1u);
    EXPECT_EQ(h.root(), root2);
    EXPECT_FALSE(h.contains(root1));
    EXPECT_FALSE(h.contains(child));
}

TEST_F(HierarchyTest, AddChildren)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child1 = make_obj();
    auto child2 = make_obj();

    h.set_root(root);
    EXPECT_TRUE(succeeded(h.add(root, child1)));
    EXPECT_TRUE(succeeded(h.add(root, child2)));

    EXPECT_EQ(h.size(), 3u);
    EXPECT_EQ(h.child_count(root), 2u);
    EXPECT_TRUE(h.contains(child1));
    EXPECT_TRUE(h.contains(child2));
}

TEST_F(HierarchyTest, AddToNonMemberParentFails)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto orphan = make_obj();
    auto child = make_obj();

    h.set_root(root);
    EXPECT_EQ(h.add(orphan, child), ReturnValue::InvalidArgument);
}

TEST_F(HierarchyTest, AddDuplicateFails)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child = make_obj();

    h.set_root(root);
    h.add(root, child);
    EXPECT_EQ(h.add(root, child), ReturnValue::InvalidArgument);
}

TEST_F(HierarchyTest, AddNullFails)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    h.set_root(root);

    EXPECT_EQ(h.add(root, {}), ReturnValue::InvalidArgument);
    EXPECT_EQ(h.add({}, root), ReturnValue::InvalidArgument);
}

TEST_F(HierarchyTest, ParentOfRoot)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    h.set_root(root);

    EXPECT_FALSE(h.parent_of(root));
}

TEST_F(HierarchyTest, ParentOfChild)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child = make_obj();

    h.set_root(root);
    h.add(root, child);

    EXPECT_EQ(h.parent_of(child), root);
}

TEST_F(HierarchyTest, ChildrenOf)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child1 = make_obj();
    auto child2 = make_obj();

    h.set_root(root);
    h.add(root, child1);
    h.add(root, child2);

    auto children = h.children_of(root);
    EXPECT_EQ(children.size(), 2u);
    EXPECT_EQ(children[0], child1);
    EXPECT_EQ(children[1], child2);
}

TEST_F(HierarchyTest, InsertAtIndex)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child1 = make_obj();
    auto child2 = make_obj();
    auto child3 = make_obj();

    h.set_root(root);
    h.add(root, child1);
    h.add(root, child3);

    EXPECT_TRUE(succeeded(h.insert(root, 1, child2)));

    auto children = h.children_of(root);
    EXPECT_EQ(children.size(), 3u);
    EXPECT_EQ(children[0], child1);
    EXPECT_EQ(children[1], child2);
    EXPECT_EQ(children[2], child3);
}

TEST_F(HierarchyTest, InsertOutOfBoundsFails)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child = make_obj();

    h.set_root(root);
    EXPECT_EQ(h.insert(root, 5, child), ReturnValue::InvalidArgument);
}

TEST_F(HierarchyTest, RemoveSubtree)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto parent = make_obj();
    auto child1 = make_obj();
    auto child2 = make_obj();

    h.set_root(root);
    h.add(root, parent);
    h.add(parent, child1);
    h.add(parent, child2);
    EXPECT_EQ(h.size(), 4u);

    EXPECT_TRUE(succeeded(h.remove(parent)));
    EXPECT_EQ(h.size(), 1u);
    EXPECT_FALSE(h.contains(parent));
    EXPECT_FALSE(h.contains(child1));
    EXPECT_FALSE(h.contains(child2));
    EXPECT_EQ(h.child_count(root), 0u);
}

TEST_F(HierarchyTest, RemoveRoot)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child = make_obj();

    h.set_root(root);
    h.add(root, child);

    EXPECT_TRUE(succeeded(h.remove(root)));
    EXPECT_EQ(h.size(), 0u);
    EXPECT_FALSE(h.root());
}

TEST_F(HierarchyTest, RemoveNonMember)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto orphan = make_obj();

    h.set_root(root);
    EXPECT_EQ(h.remove(orphan), ReturnValue::NothingToDo);
}

TEST_F(HierarchyTest, ReplaceChild)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child1 = make_obj();
    auto child2 = make_obj();
    auto replacement = make_obj();

    h.set_root(root);
    h.add(root, child1);
    h.add(root, child2);

    EXPECT_TRUE(succeeded(h.replace(child1, replacement)));
    EXPECT_FALSE(h.contains(child1));
    EXPECT_TRUE(h.contains(replacement));
    EXPECT_EQ(h.parent_of(replacement), root);

    auto children = h.children_of(root);
    EXPECT_EQ(children[0], replacement);
    EXPECT_EQ(children[1], child2);
}

TEST_F(HierarchyTest, ReplacePreservesChildren)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto parent = make_obj();
    auto grandchild = make_obj();
    auto replacement = make_obj();

    h.set_root(root);
    h.add(root, parent);
    h.add(parent, grandchild);

    h.replace(parent, replacement);

    EXPECT_EQ(h.child_count(replacement), 1u);
    EXPECT_EQ(h.parent_of(grandchild), replacement);
}

TEST_F(HierarchyTest, ReplaceRoot)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child = make_obj();
    auto new_root = make_obj();

    h.set_root(root);
    h.add(root, child);

    h.replace(root, new_root);

    EXPECT_EQ(h.root(), new_root);
    EXPECT_EQ(h.child_count(new_root), 1u);
    EXPECT_EQ(h.parent_of(child), new_root);
}

TEST_F(HierarchyTest, ReplaceNonMemberFails)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto orphan = make_obj();
    auto replacement = make_obj();

    h.set_root(root);
    EXPECT_EQ(h.replace(orphan, replacement), ReturnValue::InvalidArgument);
}

TEST_F(HierarchyTest, ReplaceDuplicateFails)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child = make_obj();

    h.set_root(root);
    h.add(root, child);

    // Can't replace child with root (root is already in hierarchy)
    EXPECT_EQ(h.replace(child, root), ReturnValue::InvalidArgument);
}

TEST_F(HierarchyTest, NodeOfReturnsSnapshot)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child1 = make_obj();
    auto child2 = make_obj();

    h.set_root(root);
    h.add(root, child1);
    h.add(root, child2);

    auto node = h.node_of(root);
    EXPECT_TRUE(node);
    EXPECT_EQ(node, root);
    EXPECT_FALSE(node.has_parent());
    EXPECT_EQ(node.child_count(), 2u);
    EXPECT_TRUE(node.hierarchy());

    auto childNode = h.node_of(child1);
    EXPECT_TRUE(childNode);
    EXPECT_EQ(childNode, child1);
    EXPECT_EQ(childNode.get_parent(), root);
    EXPECT_TRUE(childNode.has_parent());
    EXPECT_EQ(childNode.child_count(), 0u);
}

TEST_F(HierarchyTest, NodeOfNonMemberReturnsEmpty)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto orphan = make_obj();

    h.set_root(root);
    auto node = h.node_of(orphan);
    EXPECT_FALSE(node);
}

TEST_F(HierarchyTest, ContainsAndSize)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child = make_obj();

    EXPECT_EQ(h.size(), 0u);
    EXPECT_FALSE(h.contains(root));

    h.set_root(root);
    EXPECT_TRUE(h.contains(root));
    EXPECT_FALSE(h.contains(child));
    EXPECT_EQ(h.size(), 1u);

    h.add(root, child);
    EXPECT_TRUE(h.contains(child));
    EXPECT_EQ(h.size(), 2u);
}

TEST_F(HierarchyTest, Clear)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child = make_obj();

    h.set_root(root);
    h.add(root, child);

    h.clear();
    EXPECT_EQ(h.size(), 0u);
    EXPECT_TRUE(h.empty());
    EXPECT_FALSE(h.root());
    EXPECT_FALSE(h.contains(root));
}

TEST_F(HierarchyTest, ChildrenOfReturnsNodes)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child1 = make_obj();
    auto child2 = make_obj();

    h.set_root(root);
    h.add(root, child1);
    h.add(root, child2);

    auto children = h.children_of(root);
    EXPECT_EQ(children.size(), 2u);
    // Each child is a Node that can be used as an Object
    EXPECT_EQ(children[0].class_uid(), HierarchyTestObj::class_id());
    EXPECT_EQ(children[1].class_uid(), HierarchyTestObj::class_id());
}

TEST_F(HierarchyTest, ForEachChild)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child1 = make_obj();
    auto child2 = make_obj();

    h.set_root(root);
    h.add(root, child1);
    h.add(root, child2);

    // Set different values on children via state
    interface_cast<IHierarchyTest>(child1)->value().set_value(10);
    interface_cast<IHierarchyTest>(child2)->value().set_value(20);

    int sum = 0;
    h.for_each_child<IHierarchyTest>(root, [&](IHierarchyTest& obj) {
        sum += obj.value().get_value();
    });
    EXPECT_EQ(sum, 30);
}

TEST_F(HierarchyTest, ForEachChildEarlyStop)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child1 = make_obj();
    auto child2 = make_obj();

    h.set_root(root);
    h.add(root, child1);
    h.add(root, child2);

    int count = 0;
    h.for_each_child<IHierarchyTest>(root, [&](IHierarchyTest&) -> bool {
        count++;
        return false; // stop after first
    });
    EXPECT_EQ(count, 1);
}

TEST_F(HierarchyTest, NullHierarchySafe)
{
    Hierarchy h;
    EXPECT_FALSE(h);
    EXPECT_EQ(h.size(), 0u);
    EXPECT_TRUE(h.empty());
    EXPECT_FALSE(h.root());
    EXPECT_EQ(h.set_root({}), ReturnValue::InvalidArgument);
    EXPECT_EQ(h.add({}, {}), ReturnValue::InvalidArgument);
    EXPECT_EQ(h.insert({}, 0, {}), ReturnValue::InvalidArgument);
    EXPECT_EQ(h.remove({}), ReturnValue::InvalidArgument);
    EXPECT_EQ(h.replace({}, {}), ReturnValue::InvalidArgument);
    h.clear(); // should not crash
    EXPECT_FALSE(h.parent_of({}));
    EXPECT_EQ(h.children_of({}).size(), 0u);
    EXPECT_EQ(h.child_count({}), 0u);
    EXPECT_FALSE(h.contains({}));
}

TEST_F(HierarchyTest, RootReturnsNode)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child = make_obj();

    h.set_root(root);
    h.add(root, child);

    auto node = h.root();
    EXPECT_TRUE(node);
    EXPECT_EQ(node, root);
    EXPECT_FALSE(node.has_parent());
    EXPECT_EQ(node.child_count(), 1u);
    EXPECT_EQ(node.child_at(0), child);
    EXPECT_TRUE(node.hierarchy());
}

TEST_F(HierarchyTest, HierarchyClassId)
{
    auto h = create_hierarchy();
    EXPECT_EQ(h.class_uid(), ClassId::Hierarchy);
}

TEST_F(HierarchyTest, GetHierarchyInterface)
{
    auto h = create_hierarchy();
    auto iface = h.get_hierarchy_interface();
    EXPECT_TRUE(iface);
}

TEST_F(HierarchyTest, NodeInheritsObject)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    h.set_root(root);

    // Set a property value on the object
    interface_cast<IHierarchyTest>(root)->value().set_value(42);

    // Access through Node's inherited Object methods
    auto node = h.root();
    EXPECT_EQ(node.class_uid(), HierarchyTestObj::class_id());
    auto* iht = node.as<IHierarchyTest>();
    ASSERT_NE(iht, nullptr);
    EXPECT_EQ(iht->value().get_value(), 42);
}

TEST_F(HierarchyTest, NodeGetChildren)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child1 = make_obj();
    auto child2 = make_obj();

    h.set_root(root);
    h.add(root, child1);
    h.add(root, child2);

    auto node = h.root();
    auto children = node.get_children();
    EXPECT_EQ(children.size(), 2u);
    EXPECT_EQ(children[0], child1);
    EXPECT_EQ(children[1], child2);
}

TEST_F(HierarchyTest, NodeChildAt)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child = make_obj();

    h.set_root(root);
    h.add(root, child);

    auto node = h.root();
    EXPECT_EQ(node.child_at(0), child);
    EXPECT_FALSE(node.child_at(1)); // out of range
}

TEST_F(HierarchyTest, NodeTypedChildAt)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child = make_obj();

    h.set_root(root);
    h.add(root, child);

    auto node = h.root();
    auto typed = node.child_at<IHierarchyTest>(0);
    EXPECT_TRUE(typed);
    EXPECT_FALSE(node.child_at<IHierarchyTest>(5));
}

TEST_F(HierarchyTest, NodeImplicitConversion)
{
    auto h = create_hierarchy();
    auto root_obj = make_obj();
    auto child_obj = make_obj();

    h.set_root(root_obj);

    // Node converts to IObject::Ptr, so it can be used directly with Hierarchy methods
    auto root_node = h.root();
    EXPECT_TRUE(succeeded(h.add(root_node, child_obj)));
    EXPECT_EQ(h.child_count(root_node), 1u);
    EXPECT_TRUE(h.contains(root_node));
}

TEST_F(HierarchyTest, NodeForEachChild)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child1 = make_obj();
    auto child2 = make_obj();

    h.set_root(root);
    h.add(root, child1);
    h.add(root, child2);

    interface_cast<IHierarchyTest>(child1)->value().set_value(10);
    interface_cast<IHierarchyTest>(child2)->value().set_value(20);

    auto node = h.root();
    int sum = 0;
    node.for_each_child<IHierarchyTest>([&](IHierarchyTest& obj) {
        sum += obj.value().get_value();
    });
    EXPECT_EQ(sum, 30);
}

TEST_F(HierarchyTest, NodeForEachChildEarlyStop)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child1 = make_obj();
    auto child2 = make_obj();

    h.set_root(root);
    h.add(root, child1);
    h.add(root, child2);

    auto node = h.root();
    int count = 0;
    node.for_each_child<IHierarchyTest>([&](IHierarchyTest&) -> bool {
        count++;
        return false;
    });
    EXPECT_EQ(count, 1);
}

TEST_F(HierarchyTest, NodeDefaultConstructedIsEmpty)
{
    ::velk::Node node;
    EXPECT_FALSE(node);
    EXPECT_FALSE(node.object());
    EXPECT_FALSE(node.get_parent());
    EXPECT_FALSE(node.has_parent());
    EXPECT_EQ(node.child_count(), 0u);
    EXPECT_FALSE(node.child_at(0));
    EXPECT_EQ(node.get_children().size(), 0u);
}

TEST_F(HierarchyTest, NodeHierarchyNodeAccess)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    h.set_root(root);

    auto node = h.root();
    auto& hn = node.hierarchy_node();
    EXPECT_EQ(hn.object, root);
}

TEST_F(HierarchyTest, NodeReflectsLiveState)
{
    auto h = create_hierarchy();
    auto root = make_obj();
    auto child1 = make_obj();

    h.set_root(root);
    h.add(root, child1);

    // Get node before mutation
    auto node = h.root();
    EXPECT_EQ(node.child_count(), 1u);

    // Mutate hierarchy after node was created
    auto child2 = make_obj();
    h.add(root, child2);

    // Node queries hierarchy on demand, so it sees the new child
    EXPECT_EQ(node.child_count(), 2u);
    EXPECT_EQ(node.child_at(1), child2);
}
