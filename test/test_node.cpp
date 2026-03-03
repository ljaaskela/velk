#include <velk/api/node.h>
#include <velk/api/velk.h>
#include <velk/ext/object.h>

#include <gtest/gtest.h>

using namespace velk;

class INodeTest : public Interface<INodeTest>
{
public:
    VELK_INTERFACE(
        (PROP, int, value, 99)
    )
};

class NodeTestObj : public ext::Object<NodeTestObj, INodeTest>
{};

class NodeTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite() { instance().type_registry().register_type<NodeTestObj>(); }

    IObject::Ptr make_child() { return instance().create<IObject>(NodeTestObj::class_id()); }
};

// ============================================================
// Node creation
// ============================================================

TEST_F(NodeTest, CreateNode)
{
    auto node = create_node();
    EXPECT_TRUE(node);
    EXPECT_EQ(node.class_uid(), ClassId::Node);
}

TEST_F(NodeTest, NodeImplementsIContainer)
{
    auto node = create_node();
    EXPECT_NE(node.as<IContainer>(), nullptr);
}

TEST_F(NodeTest, WrapperRejectsNonNode)
{
    auto obj = instance().create<IObject>(NodeTestObj::class_id());
    Node node(obj);
    EXPECT_FALSE(node);
}

// ============================================================
// Read on fresh node (no ContainerImpl yet)
// ============================================================

TEST_F(NodeTest, FreshNodeIsEmpty)
{
    auto node = create_node();
    EXPECT_EQ(node.size(), 0u);
    EXPECT_TRUE(node.empty());
    EXPECT_FALSE(node.get_at(0));
    EXPECT_TRUE(node.get_all().empty());
}

// ============================================================
// CRUD operations
// ============================================================

TEST_F(NodeTest, AddAndSize)
{
    auto node = create_node();
    auto child = make_child();

    EXPECT_EQ(node.add(child), ReturnValue::Success);
    EXPECT_EQ(node.size(), 1u);
    EXPECT_FALSE(node.empty());
}

TEST_F(NodeTest, AddNullReturnsInvalidArgument)
{
    auto node = create_node();
    EXPECT_EQ(node.add(nullptr), ReturnValue::InvalidArgument);
}

TEST_F(NodeTest, GetAt)
{
    auto node = create_node();
    auto child = make_child();
    node.add(child);

    auto retrieved = node.get_at(0);
    EXPECT_EQ(retrieved.get(), child.get());
}

TEST_F(NodeTest, GetAtOutOfRange)
{
    auto node = create_node();
    EXPECT_FALSE(node.get_at(0));

    node.add(make_child());
    EXPECT_FALSE(node.get_at(5));
}

TEST_F(NodeTest, GetAll)
{
    auto node = create_node();
    auto a = make_child();
    auto b = make_child();
    node.add(a);
    node.add(b);

    auto all = node.get_all();
    EXPECT_EQ(all.size(), 2u);
    EXPECT_EQ(all[0].get(), a.get());
    EXPECT_EQ(all[1].get(), b.get());
}

TEST_F(NodeTest, Remove)
{
    auto node = create_node();
    auto child = make_child();
    node.add(child);

    EXPECT_EQ(node.remove(child), ReturnValue::Success);
    EXPECT_EQ(node.size(), 0u);
}

TEST_F(NodeTest, RemoveOnFreshNodeReturnsNothingToDo)
{
    auto node = create_node();
    EXPECT_EQ(node.remove(make_child()), ReturnValue::NothingToDo);
}

TEST_F(NodeTest, Insert)
{
    auto node = create_node();
    auto a = make_child();
    auto b = make_child();
    auto c = make_child();

    node.add(a);
    node.add(c);
    EXPECT_EQ(node.insert(1, b), ReturnValue::Success);
    EXPECT_EQ(node.size(), 3u);
    EXPECT_EQ(node.get_at(0).get(), a.get());
    EXPECT_EQ(node.get_at(1).get(), b.get());
    EXPECT_EQ(node.get_at(2).get(), c.get());
}

TEST_F(NodeTest, Replace)
{
    auto node = create_node();
    auto a = make_child();
    auto b = make_child();
    node.add(a);

    EXPECT_EQ(node.replace(0, b), ReturnValue::Success);
    EXPECT_EQ(node.get_at(0).get(), b.get());
}

TEST_F(NodeTest, ReplaceOnFreshNodeReturnsInvalidArgument)
{
    auto node = create_node();
    EXPECT_EQ(node.replace(0, make_child()), ReturnValue::InvalidArgument);
}

TEST_F(NodeTest, Clear)
{
    auto node = create_node();
    node.add(make_child());
    node.add(make_child());
    EXPECT_EQ(node.size(), 2u);

    node.clear();
    EXPECT_EQ(node.size(), 0u);
}

TEST_F(NodeTest, ClearOnFreshNodeIsNoop)
{
    auto node = create_node();
    node.clear();
    EXPECT_EQ(node.size(), 0u);
}

// ============================================================
// Typed access
// ============================================================

TEST_F(NodeTest, TypedGetAt)
{
    auto node = create_node();
    node.add(make_child());

    auto typed = node.get_at<INodeTest>(0);
    ASSERT_TRUE(typed);
    EXPECT_EQ(typed->value().get_value(), 99);
}

TEST_F(NodeTest, TypedGetAll)
{
    auto node = create_node();
    node.add(make_child());
    node.add(make_child());

    auto all = node.get_all<INodeTest>();
    EXPECT_EQ(all.size(), 2u);
    for (auto& item : all) {
        ASSERT_TRUE(item);
    }
}

TEST_F(NodeTest, ForEach)
{
    auto node = create_node();
    node.add(make_child());
    node.add(make_child());
    node.add(make_child());

    int count = 0;
    node.for_each<INodeTest>([&](INodeTest& obj) {
        EXPECT_EQ(obj.value().get_value(), 99);
        count++;
    });
    EXPECT_EQ(count, 3);
}

TEST_F(NodeTest, ForEachEarlyStop)
{
    auto node = create_node();
    node.add(make_child());
    node.add(make_child());
    node.add(make_child());

    int count = 0;
    node.for_each<INodeTest>([&](INodeTest&) -> bool {
        count++;
        return count < 2;
    });
    EXPECT_EQ(count, 2);
}

// ============================================================
// Attachment to another object
// ============================================================

TEST_F(NodeTest, AttachNodeToObject)
{
    auto parent = instance().create<IObject>(NodeTestObj::class_id());
    auto node = find_or_create_attachment<IContainer>(parent.get(), ClassId::Node);
    ASSERT_TRUE(node);

    auto child = make_child();
    EXPECT_EQ(node->add(child), ReturnValue::Success);
    EXPECT_EQ(node->size(), 1u);
}

TEST_F(NodeTest, FindOrCreateAttachmentIdempotent)
{
    auto parent = instance().create<IObject>(NodeTestObj::class_id());
    auto first = find_or_create_attachment<IContainer>(parent.get(), ClassId::Node);
    auto second = find_or_create_attachment<IContainer>(parent.get(), ClassId::Node);
    EXPECT_EQ(first.get(), second.get());
}

// ============================================================
// Null safety
// ============================================================

TEST_F(NodeTest, NullNodeIsSafe)
{
    Node node;
    auto child = make_child();

    EXPECT_EQ(node.size(), 0u);
    EXPECT_TRUE(node.empty());
    EXPECT_EQ(node.add(child), ReturnValue::InvalidArgument);
    EXPECT_EQ(node.remove(child), ReturnValue::InvalidArgument);
    EXPECT_EQ(node.insert(0, child), ReturnValue::InvalidArgument);
    EXPECT_EQ(node.replace(0, child), ReturnValue::InvalidArgument);
    EXPECT_FALSE(node.get_at(0));
    EXPECT_TRUE(node.get_all().empty());
}
