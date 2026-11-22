#include <velk/api/callback.h>
#include <velk/api/hierarchy.h>
#include <velk/api/velk.h>
#include <velk/ext/object.h>
#include <velk/plugins/data/api/data.h>

#include <gtest/gtest.h>

#include <vector>

using namespace velk;

namespace {

class IDataTestItem : public Interface<IDataTestItem>
{
public:
    VELK_INTERFACE(
        (PROP, int, value, 0)
    )
};

class DataTestItem : public ext::Object<DataTestItem, IDataTestItem>
{};

class DataTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        instance().plugin_registry().load_plugin_from_path(TEST_DATA_DLL_PATH);
        instance().type_registry().register_type<DataTestItem>();
    }
    static void TearDownTestSuite() { instance().type_registry().unregister_type<DataTestItem>(); }

    void SetUp() override { ASSERT_TRUE(instance().plugin_registry().find_plugin(PluginId::DataPlugin)); }

    IObject::Ptr item() { return instance().create<IObject>(DataTestItem::static_class_id()); }

    void watch(IItemSource& source)
    {
        source.on_items_changed().add_handler(
            create_callback([this](const ItemChange& change) { changes.push_back(change); }));
    }

    std::vector<ItemChange> changes;
};

using T = ItemChange::Type;

TEST_F(DataTest, ListInsertRemoveFireRanges)
{
    auto list = create_object_list();
    ASSERT_TRUE(list);
    watch(*list);
    auto a = item();
    auto b = item();
    auto c = item();

    list->add(a);
    const IObject::Ptr two[] = {b, c};
    list->insert(0, two);
    ASSERT_EQ(list->size(), 3u);
    EXPECT_EQ(list->at(0), b);
    EXPECT_EQ(list->at(1), c);
    EXPECT_EQ(list->at(2), a);
    EXPECT_EQ(list->child_count({}), 3u);
    EXPECT_EQ(list->child_at({}, 2), a);
    EXPECT_EQ(list->child_count(a), 0u);
    EXPECT_FALSE(list->has_children(a));
    EXPECT_EQ(list->index_of(c), 1u);

    list->remove(1, 2);
    ASSERT_EQ(list->size(), 1u);
    EXPECT_EQ(list->at(0), b);

    ASSERT_EQ(changes.size(), 3u);
    EXPECT_EQ(changes[0], (ItemChange{T::Inserted, {}, 0, 1}));
    EXPECT_EQ(changes[1], (ItemChange{T::Inserted, {}, 0, 2}));
    EXPECT_EQ(changes[2], (ItemChange{T::Removed, {}, 1, 2}));
}

TEST_F(DataTest, ListMoveAndClear)
{
    auto list = create_object_list();
    auto a = item();
    auto b = item();
    auto c = item();
    list->add(a);
    list->add(b);
    list->add(c);
    watch(*list);

    list->move(0, 2);
    EXPECT_EQ(list->at(0), b);
    EXPECT_EQ(list->at(1), c);
    EXPECT_EQ(list->at(2), a);
    list->clear();
    EXPECT_EQ(list->size(), 0u);

    ASSERT_EQ(changes.size(), 3u);
    EXPECT_EQ(changes[0], (ItemChange{T::Removed, {}, 0, 1}));
    EXPECT_EQ(changes[1], (ItemChange{T::Inserted, {}, 2, 1}));
    EXPECT_EQ(changes[2].type, T::Reset);
}

TEST_F(DataTest, ListRefusesBadArguments)
{
    auto list = create_object_list();
    list->add(item());
    const IObject::Ptr one[] = {item()};
    EXPECT_EQ(list->insert(5, one), ReturnValue::InvalidArgument);
    EXPECT_EQ(list->remove(0, 2), ReturnValue::InvalidArgument);
    EXPECT_EQ(list->add({}), ReturnValue::InvalidArgument);
    EXPECT_EQ(list->size(), 1u);
}

TEST_F(DataTest, HierarchySourceShowsTheTree)
{
    auto h = create_hierarchy();
    auto root = item();
    auto a = item();
    auto b = item();
    h.set_root(root);
    h.add(root, a);
    h.add(a, b);

    auto source = create_hierarchy_source(h);
    ASSERT_TRUE(source);
    EXPECT_EQ(source->child_count({}), 1u);
    EXPECT_EQ(source->child_at({}, 0), root);
    EXPECT_EQ(source->child_count(root), 1u);
    EXPECT_EQ(source->child_at(root, 0), a);
    EXPECT_TRUE(source->has_children(a));
    EXPECT_FALSE(source->has_children(b));
}

TEST_F(DataTest, HierarchySourceMapsChanges)
{
    auto h = create_hierarchy();
    auto root = item();
    auto a = item();
    auto b = item();
    auto c = item();
    h.set_root(root);
    h.add(root, a);
    h.add(root, b);

    auto source = create_hierarchy_source(h);
    watch(*source);

    h.add(root, c);                                         // root: a b c
    h.remove(b);                                            // root: a c
    auto d = item();
    h.insert(root, 1, d);                                   // root: a d c
    auto e = item();
    interface_cast<IHierarchy>(h.get())->replace(a, e);     // root: e d c
    h.clear();

    ASSERT_EQ(changes.size(), 6u);
    EXPECT_EQ(changes[0], (ItemChange{T::Inserted, root, 2, 1}));
    EXPECT_EQ(changes[1], (ItemChange{T::Removed, root, 1, 1}));
    EXPECT_EQ(changes[2], (ItemChange{T::Inserted, root, 1, 1}));
    EXPECT_EQ(changes[3], (ItemChange{T::Removed, root, 0, 1}));
    EXPECT_EQ(changes[4], (ItemChange{T::Inserted, root, 0, 1}));
    EXPECT_EQ(changes[5].type, T::Reset);
}

TEST_F(DataTest, HierarchySourceResetsOnNewHierarchy)
{
    auto source = create_hierarchy_source({});
    watch(*source);
    EXPECT_EQ(source->child_count({}), 0u);

    auto h = create_hierarchy();
    h.set_root(item());
    write_state<IHierarchySource>(source, [&](IHierarchySource::State& s) { set_object_ref(s.hierarchy, IHierarchy::Ptr(h)); });
    ASSERT_EQ(changes.size(), 1u);
    EXPECT_EQ(changes[0].type, T::Reset);
    EXPECT_EQ(source->child_count({}), 1u);
}

} // namespace
