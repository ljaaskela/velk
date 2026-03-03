#include <velk/api/container.h>
#include <velk/api/velk.h>
#include <velk/ext/object.h>
#include <velk/interface/intf_container.h>
#include <velk/interface/intf_object_storage.h>

#include <gtest/gtest.h>

using namespace velk;

// Test interface with a property for verifying metadata still works with attachments
class IAttachTest : public Interface<IAttachTest>
{
public:
    VELK_INTERFACE(
        (PROP, int, value, 42)
    )
};

class AttachTestObj : public ext::Object<AttachTestObj, IAttachTest>
{};

class ContainerTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite() { instance().type_registry().register_type<AttachTestObj>(); }
};

// ============================================================
// Attachment tests
// ============================================================

TEST_F(ContainerTest, AddAndGetAttachment)
{
    auto obj = instance().create<IObject>(AttachTestObj::class_id());
    auto* storage = interface_cast<IObjectStorage>(obj);
    ASSERT_NE(storage, nullptr);

    EXPECT_EQ(storage->attachment_count(), 0u);

    auto container = instance().create<IInterface>(ClassId::Container);
    ASSERT_TRUE(container);

    EXPECT_EQ(storage->add_attachment(container), ReturnValue::Success);
    EXPECT_EQ(storage->attachment_count(), 1u);

    auto retrieved = storage->get_attachment(0);
    EXPECT_EQ(retrieved.get(), container.get());
}

TEST_F(ContainerTest, RemoveAttachment)
{
    auto obj = instance().create<IObject>(AttachTestObj::class_id());
    auto* storage = interface_cast<IObjectStorage>(obj);
    ASSERT_NE(storage, nullptr);

    auto container = instance().create<IInterface>(ClassId::Container);
    storage->add_attachment(container);
    EXPECT_EQ(storage->attachment_count(), 1u);

    EXPECT_EQ(storage->remove_attachment(container), ReturnValue::Success);
    EXPECT_EQ(storage->attachment_count(), 0u);
}

TEST_F(ContainerTest, RemoveNonexistentAttachmentReturnsNothingToDo)
{
    auto obj = instance().create<IObject>(AttachTestObj::class_id());
    auto* storage = interface_cast<IObjectStorage>(obj);
    ASSERT_NE(storage, nullptr);

    auto container = instance().create<IInterface>(ClassId::Container);
    EXPECT_EQ(storage->remove_attachment(container), ReturnValue::NothingToDo);
}

TEST_F(ContainerTest, AddNullAttachmentReturnsInvalidArgument)
{
    auto obj = instance().create<IObject>(AttachTestObj::class_id());
    auto* storage = interface_cast<IObjectStorage>(obj);
    ASSERT_NE(storage, nullptr);

    EXPECT_EQ(storage->add_attachment(nullptr), ReturnValue::InvalidArgument);
}

TEST_F(ContainerTest, RemoveNullAttachmentReturnsInvalidArgument)
{
    auto obj = instance().create<IObject>(AttachTestObj::class_id());
    auto* storage = interface_cast<IObjectStorage>(obj);
    ASSERT_NE(storage, nullptr);

    EXPECT_EQ(storage->remove_attachment(nullptr), ReturnValue::InvalidArgument);
}

TEST_F(ContainerTest, GetAttachmentOutOfRangeReturnsNull)
{
    auto obj = instance().create<IObject>(AttachTestObj::class_id());
    auto* storage = interface_cast<IObjectStorage>(obj);
    ASSERT_NE(storage, nullptr);

    EXPECT_FALSE(storage->get_attachment(0));
    EXPECT_FALSE(storage->get_attachment(99));
}

TEST_F(ContainerTest, FindAttachmentByType)
{
    auto obj = instance().create<IObject>(AttachTestObj::class_id());
    auto* storage = interface_cast<IObjectStorage>(obj);
    ASSERT_NE(storage, nullptr);

    auto container = instance().create<IInterface>(ClassId::Container);
    storage->add_attachment(container);

    auto found = storage->find_attachment<IContainer>();
    EXPECT_TRUE(found);
    EXPECT_EQ(found.get(), interface_cast<IContainer>(container));
}

TEST_F(ContainerTest, FindAttachmentReturnsNullWhenNotFound)
{
    auto obj = instance().create<IObject>(AttachTestObj::class_id());
    auto* storage = interface_cast<IObjectStorage>(obj);
    ASSERT_NE(storage, nullptr);

    auto found = storage->find_attachment<IContainer>();
    EXPECT_FALSE(found);
}

TEST_F(ContainerTest, MetadataStillWorksAfterAttaching)
{
    auto obj = instance().create<IObject>(AttachTestObj::class_id());
    auto* iface = interface_cast<IAttachTest>(obj);
    ASSERT_NE(iface, nullptr);

    // Access property before attachment
    EXPECT_EQ(iface->value().get_value(), 42);

    // Attach something
    auto* storage = interface_cast<IObjectStorage>(obj);
    ASSERT_NE(storage, nullptr);
    auto container = instance().create<IInterface>(ClassId::Container);
    storage->add_attachment(container);

    // Property still works
    iface->value().set_value(99);
    EXPECT_EQ(iface->value().get_value(), 99);

    // Attachment still accessible
    EXPECT_EQ(storage->attachment_count(), 1u);
}

TEST_F(ContainerTest, MetadataWorksWhenAttachedBeforePropertyAccess)
{
    auto obj = instance().create<IObject>(AttachTestObj::class_id());
    auto* storage = interface_cast<IObjectStorage>(obj);
    ASSERT_NE(storage, nullptr);

    // Attach first
    auto container = instance().create<IInterface>(ClassId::Container);
    storage->add_attachment(container);

    // Then access property
    auto* iface = interface_cast<IAttachTest>(obj);
    ASSERT_NE(iface, nullptr);
    EXPECT_EQ(iface->value().get_value(), 42);
}

// ============================================================
// IContainer tests
// ============================================================

TEST_F(ContainerTest, CreateContainer)
{
    auto container = instance().create<IContainer>(ClassId::Container);
    ASSERT_TRUE(container);
    EXPECT_EQ(container->size(), 0u);
}

TEST_F(ContainerTest, AddAndGetAt)
{
    auto container = instance().create<IContainer>(ClassId::Container);
    auto child = instance().create<IObject>(AttachTestObj::class_id());
    ASSERT_TRUE(container);
    ASSERT_TRUE(child);

    EXPECT_EQ(container->add(child), ReturnValue::Success);
    EXPECT_EQ(container->size(), 1u);

    auto retrieved = container->get_at(0);
    EXPECT_EQ(retrieved.get(), child.get());
}

TEST_F(ContainerTest, AddNullReturnsInvalidArgument)
{
    auto container = instance().create<IContainer>(ClassId::Container);
    ASSERT_TRUE(container);
    EXPECT_EQ(container->add(nullptr), ReturnValue::InvalidArgument);
}

TEST_F(ContainerTest, RemoveChild)
{
    auto container = instance().create<IContainer>(ClassId::Container);
    auto child = instance().create<IObject>(AttachTestObj::class_id());
    container->add(child);
    EXPECT_EQ(container->size(), 1u);

    EXPECT_EQ(container->remove(child), ReturnValue::Success);
    EXPECT_EQ(container->size(), 0u);
}

TEST_F(ContainerTest, RemoveNonexistentChildReturnsNothingToDo)
{
    auto container = instance().create<IContainer>(ClassId::Container);
    auto child = instance().create<IObject>(AttachTestObj::class_id());
    EXPECT_EQ(container->remove(child), ReturnValue::NothingToDo);
}

TEST_F(ContainerTest, InsertChild)
{
    auto container = instance().create<IContainer>(ClassId::Container);
    auto a = instance().create<IObject>(AttachTestObj::class_id());
    auto b = instance().create<IObject>(AttachTestObj::class_id());
    auto c = instance().create<IObject>(AttachTestObj::class_id());

    container->add(a);
    container->add(c);
    EXPECT_EQ(container->insert(1, b), ReturnValue::Success);

    EXPECT_EQ(container->size(), 3u);
    EXPECT_EQ(container->get_at(0).get(), a.get());
    EXPECT_EQ(container->get_at(1).get(), b.get());
    EXPECT_EQ(container->get_at(2).get(), c.get());
}

TEST_F(ContainerTest, InsertOutOfBoundsReturnsInvalidArgument)
{
    auto container = instance().create<IContainer>(ClassId::Container);
    auto child = instance().create<IObject>(AttachTestObj::class_id());
    EXPECT_EQ(container->insert(5, child), ReturnValue::InvalidArgument);
}

TEST_F(ContainerTest, InsertNullReturnsInvalidArgument)
{
    auto container = instance().create<IContainer>(ClassId::Container);
    EXPECT_EQ(container->insert(0, nullptr), ReturnValue::InvalidArgument);
}

TEST_F(ContainerTest, ReplaceChild)
{
    auto container = instance().create<IContainer>(ClassId::Container);
    auto a = instance().create<IObject>(AttachTestObj::class_id());
    auto b = instance().create<IObject>(AttachTestObj::class_id());

    container->add(a);
    EXPECT_EQ(container->replace(0, b), ReturnValue::Success);
    EXPECT_EQ(container->get_at(0).get(), b.get());
}

TEST_F(ContainerTest, ReplaceOutOfBoundsReturnsInvalidArgument)
{
    auto container = instance().create<IContainer>(ClassId::Container);
    auto child = instance().create<IObject>(AttachTestObj::class_id());
    EXPECT_EQ(container->replace(0, child), ReturnValue::InvalidArgument);
}

TEST_F(ContainerTest, ReplaceNullReturnsInvalidArgument)
{
    auto container = instance().create<IContainer>(ClassId::Container);
    auto child = instance().create<IObject>(AttachTestObj::class_id());
    container->add(child);
    EXPECT_EQ(container->replace(0, nullptr), ReturnValue::InvalidArgument);
}

TEST_F(ContainerTest, GetAtOutOfRangeReturnsNull)
{
    auto container = instance().create<IContainer>(ClassId::Container);
    EXPECT_FALSE(container->get_at(0));
}

TEST_F(ContainerTest, GetAll)
{
    auto container = instance().create<IContainer>(ClassId::Container);
    auto a = instance().create<IObject>(AttachTestObj::class_id());
    auto b = instance().create<IObject>(AttachTestObj::class_id());

    container->add(a);
    container->add(b);

    auto all = container->get_all();
    EXPECT_EQ(all.size(), 2u);
    EXPECT_EQ(all[0].get(), a.get());
    EXPECT_EQ(all[1].get(), b.get());
}

TEST_F(ContainerTest, Clear)
{
    auto container = instance().create<IContainer>(ClassId::Container);
    auto a = instance().create<IObject>(AttachTestObj::class_id());
    auto b = instance().create<IObject>(AttachTestObj::class_id());

    container->add(a);
    container->add(b);
    container->clear();

    EXPECT_EQ(container->size(), 0u);
}

TEST_F(ContainerTest, OrderPreservation)
{
    auto container = instance().create<IContainer>(ClassId::Container);
    auto a = instance().create<IObject>(AttachTestObj::class_id());
    auto b = instance().create<IObject>(AttachTestObj::class_id());
    auto c = instance().create<IObject>(AttachTestObj::class_id());

    container->add(a);
    container->add(b);
    container->add(c);

    EXPECT_EQ(container->get_at(0).get(), a.get());
    EXPECT_EQ(container->get_at(1).get(), b.get());
    EXPECT_EQ(container->get_at(2).get(), c.get());
}

// ============================================================
// find_or_create_attachment tests
// ============================================================

TEST_F(ContainerTest, FindOrCreateAttachmentCreatesOnMiss)
{
    auto obj = instance().create<IObject>(AttachTestObj::class_id());
    auto* storage = interface_cast<IObjectStorage>(obj);
    ASSERT_NE(storage, nullptr);

    EXPECT_EQ(storage->attachment_count(), 0u);

    auto container = find_or_create_attachment<IContainer>(storage, ClassId::Container);
    ASSERT_TRUE(container);
    EXPECT_EQ(storage->attachment_count(), 1u);
}

TEST_F(ContainerTest, FindOrCreateAttachmentReturnsSameOnHit)
{
    auto obj = instance().create<IObject>(AttachTestObj::class_id());
    auto* storage = interface_cast<IObjectStorage>(obj);
    ASSERT_NE(storage, nullptr);

    auto c1 = find_or_create_attachment<IContainer>(storage, ClassId::Container);
    auto c2 = find_or_create_attachment<IContainer>(storage, ClassId::Container);
    ASSERT_TRUE(c1);
    ASSERT_TRUE(c2);
    EXPECT_EQ(c1.get(), c2.get());
    EXPECT_EQ(storage->attachment_count(), 1u);
}

TEST_F(ContainerTest, FindOrCreateAttachmentViaIInterface)
{
    auto obj = instance().create<IObject>(AttachTestObj::class_id());
    auto container = find_or_create_attachment<IContainer>(obj.get(), ClassId::Container);
    ASSERT_TRUE(container);

    // Second call returns the same instance
    auto container2 = find_or_create_attachment<IContainer>(obj.get(), ClassId::Container);
    EXPECT_EQ(container.get(), container2.get());
}

TEST_F(ContainerTest, FindOrCreateAttachmentOnNullReturnsNull)
{
    IObjectStorage* null_storage = nullptr;
    EXPECT_FALSE(find_or_create_attachment<IContainer>(null_storage, ClassId::Container));

    IInterface* null_obj = nullptr;
    EXPECT_FALSE(find_or_create_attachment<IContainer>(null_obj, ClassId::Container));
}

// ============================================================
// Container wrapper tests
// ============================================================

TEST_F(ContainerTest, WrapperBoolAndEmpty)
{
    Container valid(instance().create<IContainer>(ClassId::Container));
    EXPECT_TRUE(valid);
    EXPECT_TRUE(valid.empty());
    EXPECT_EQ(valid.size(), 0u);

    Container invalid;
    EXPECT_FALSE(invalid);
    EXPECT_TRUE(invalid.empty());
    EXPECT_EQ(invalid.size(), 0u);
}

TEST_F(ContainerTest, WrapperAddAndGetAt)
{
    Container c(instance().create<IContainer>(ClassId::Container));
    auto child = instance().create<IObject>(AttachTestObj::class_id());

    EXPECT_EQ(c.add(child), ReturnValue::Success);
    EXPECT_EQ(c.size(), 1u);
    EXPECT_FALSE(c.empty());

    auto retrieved = c.get_at(0);
    EXPECT_EQ(retrieved.get(), child.get());
}

TEST_F(ContainerTest, WrapperRemove)
{
    Container c(instance().create<IContainer>(ClassId::Container));
    auto child = instance().create<IObject>(AttachTestObj::class_id());
    c.add(child);

    EXPECT_EQ(c.remove(child), ReturnValue::Success);
    EXPECT_EQ(c.size(), 0u);
}

TEST_F(ContainerTest, WrapperInsertAndReplace)
{
    Container c(instance().create<IContainer>(ClassId::Container));
    auto a = instance().create<IObject>(AttachTestObj::class_id());
    auto b = instance().create<IObject>(AttachTestObj::class_id());
    auto d = instance().create<IObject>(AttachTestObj::class_id());

    c.add(a);
    c.add(d);
    EXPECT_EQ(c.insert(1, b), ReturnValue::Success);
    EXPECT_EQ(c.size(), 3u);
    EXPECT_EQ(c.get_at(1).get(), b.get());

    EXPECT_EQ(c.replace(2, a), ReturnValue::Success);
    EXPECT_EQ(c.get_at(2).get(), a.get());
}

TEST_F(ContainerTest, WrapperGetAll)
{
    Container c(instance().create<IContainer>(ClassId::Container));
    auto a = instance().create<IObject>(AttachTestObj::class_id());
    auto b = instance().create<IObject>(AttachTestObj::class_id());
    c.add(a);
    c.add(b);

    auto all = c.get_all();
    EXPECT_EQ(all.size(), 2u);
    EXPECT_EQ(all[0].get(), a.get());
    EXPECT_EQ(all[1].get(), b.get());
}

TEST_F(ContainerTest, WrapperClear)
{
    Container c(instance().create<IContainer>(ClassId::Container));
    c.add(instance().create<IObject>(AttachTestObj::class_id()));
    c.add(instance().create<IObject>(AttachTestObj::class_id()));
    EXPECT_EQ(c.size(), 2u);

    c.clear();
    EXPECT_EQ(c.size(), 0u);
}

TEST_F(ContainerTest, WrapperForEach)
{
    Container c(instance().create<IContainer>(ClassId::Container));
    c.add(instance().create<IObject>(AttachTestObj::class_id()));
    c.add(instance().create<IObject>(AttachTestObj::class_id()));
    c.add(instance().create<IObject>(AttachTestObj::class_id()));

    int count = 0;
    c.for_each<IObject>([&](IObject&) { count++; });
    EXPECT_EQ(count, 3);
}

TEST_F(ContainerTest, WrapperForEachEarlyStop)
{
    Container c(instance().create<IContainer>(ClassId::Container));
    c.add(instance().create<IObject>(AttachTestObj::class_id()));
    c.add(instance().create<IObject>(AttachTestObj::class_id()));
    c.add(instance().create<IObject>(AttachTestObj::class_id()));

    int count = 0;
    c.for_each<IObject>([&](IObject&) -> bool {
        count++;
        return count < 2; // stop after 2
    });
    EXPECT_EQ(count, 2);
}

TEST_F(ContainerTest, WrapperNullSafe)
{
    Container c;
    auto child = instance().create<IObject>(AttachTestObj::class_id());

    EXPECT_EQ(c.add(child), ReturnValue::InvalidArgument);
    EXPECT_EQ(c.remove(child), ReturnValue::InvalidArgument);
    EXPECT_EQ(c.insert(0, child), ReturnValue::InvalidArgument);
    EXPECT_EQ(c.replace(0, child), ReturnValue::InvalidArgument);
    EXPECT_FALSE(c.get_at(0));
    EXPECT_TRUE(c.get_all().empty());

    int count = 0;
    c.for_each<IObject>([&](IObject&) { count++; });
    EXPECT_EQ(count, 0);
}

TEST_F(ContainerTest, TypedWrapperGetAt)
{
    Container c(instance().create<IContainer>(ClassId::Container));
    auto child = instance().create<IObject>(AttachTestObj::class_id());
    c.add(child);

    auto typed = c.get_at<IAttachTest>(0);
    ASSERT_TRUE(typed);
    EXPECT_EQ(typed->value().get_value(), 42);
}

TEST_F(ContainerTest, TypedWrapperForEach)
{
    Container c(instance().create<IContainer>(ClassId::Container));
    c.add(instance().create<IObject>(AttachTestObj::class_id()));
    c.add(instance().create<IObject>(AttachTestObj::class_id()));

    int count = 0;
    c.for_each<IAttachTest>([&](IAttachTest& obj) {
        EXPECT_EQ(obj.value().get_value(), 42);
        count++;
    });
    EXPECT_EQ(count, 2);
}

TEST_F(ContainerTest, TypedWrapperGetAll)
{
    Container c(instance().create<IContainer>(ClassId::Container));
    c.add(instance().create<IObject>(AttachTestObj::class_id()));
    c.add(instance().create<IObject>(AttachTestObj::class_id()));

    auto all = c.get_all<IAttachTest>();
    EXPECT_EQ(all.size(), 2u);
    for (auto& item : all) {
        ASSERT_TRUE(item);
    }
}

TEST_F(ContainerTest, WrapperGetContainer)
{
    auto raw = instance().create<IContainer>(ClassId::Container);
    Container c(raw);
    EXPECT_EQ(c.get_container_interface().get(), raw.get());
}

TEST_F(ContainerTest, ContainerInheritsObject)
{
    auto raw = instance().create<IContainer>(ClassId::Container);
    Container c(raw);

    // Object methods should work through Container
    EXPECT_TRUE(c);
    EXPECT_EQ(c.class_uid(), ClassId::Container);
    EXPECT_FALSE(c.class_name().empty());
}

TEST_F(ContainerTest, ContainerFromIObjectPtr)
{
    // Create a container object and wrap it via IObject::Ptr
    auto obj = instance().create<IObject>(ClassId::Container);
    ASSERT_TRUE(obj);

    Container c(obj);
    EXPECT_TRUE(c);
    EXPECT_EQ(c.class_uid(), ClassId::Container);
    EXPECT_EQ(c.size(), 0u);

    // Container interface should be lazily resolved
    EXPECT_NE(c.get_container_interface(), nullptr);
}

TEST_F(ContainerTest, DefaultConstructedContainerIsInvalid)
{
    Container c;
    EXPECT_FALSE(c);
    EXPECT_EQ(c.size(), 0u);
    EXPECT_TRUE(c.empty());
    EXPECT_EQ(c.get_container_interface(), nullptr);
}
