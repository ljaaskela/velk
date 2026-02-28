#include <velk/api/any.h>
#include <velk/api/callback.h>
#include <velk/api/property.h>
#include <velk/api/velk.h>
#include <velk/ext/object.h>
#include <velk/interface/intf_array_property.h>
#include <velk/interface/intf_metadata.h>
#include <velk/vector.h>

#include <gtest/gtest.h>

using namespace velk;

// Test interface with array properties

class IArrayWidget : public Interface<IArrayWidget>
{
public:
    VELK_INTERFACE(
        (ARR, float, items),
        (ARR, float, presets, 1.f, 2.f, 3.f),
        (RARR, int32_t, ids, 10, 20),
        (PROP, int, count, 0)
    )
};

class ArrayWidget : public ext::Object<ArrayWidget, IArrayWidget>
{};

class ArrayPropertyTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite() { instance().type_registry().register_type<ArrayWidget>(); }
};

// Basic creation and defaults

TEST_F(ArrayPropertyTest, EmptyArrayDefault)
{
    auto obj = instance().create<IObject>(ArrayWidget::class_id());
    auto* iw = interface_cast<IArrayWidget>(obj);
    ASSERT_NE(iw, nullptr);

    auto items = iw->items();
    EXPECT_TRUE(items);
    EXPECT_EQ(items.size(), 0u);
    EXPECT_TRUE(items.empty());
}

TEST_F(ArrayPropertyTest, NonEmptyDefault)
{
    auto obj = instance().create<IObject>(ArrayWidget::class_id());
    auto* iw = interface_cast<IArrayWidget>(obj);
    ASSERT_NE(iw, nullptr);

    auto presets = iw->presets();
    EXPECT_TRUE(presets);
    EXPECT_EQ(presets.size(), 3u);
    EXPECT_FALSE(presets.empty());

    EXPECT_FLOAT_EQ(presets.at(0), 1.f);
    EXPECT_FLOAT_EQ(presets.at(1), 2.f);
    EXPECT_FLOAT_EQ(presets.at(2), 3.f);
}

// Element access

TEST_F(ArrayPropertyTest, PushBackAndAt)
{
    auto obj = instance().create<IObject>(ArrayWidget::class_id());
    auto* iw = interface_cast<IArrayWidget>(obj);
    ASSERT_NE(iw, nullptr);

    auto items = iw->items();
    EXPECT_EQ(items.push_back(10.f), ReturnValue::Success);
    EXPECT_EQ(items.push_back(20.f), ReturnValue::Success);
    EXPECT_EQ(items.push_back(30.f), ReturnValue::Success);

    EXPECT_EQ(items.size(), 3u);
    EXPECT_FLOAT_EQ(items.at(0), 10.f);
    EXPECT_FLOAT_EQ(items.at(1), 20.f);
    EXPECT_FLOAT_EQ(items.at(2), 30.f);
}

TEST_F(ArrayPropertyTest, SetAt)
{
    auto obj = instance().create<IObject>(ArrayWidget::class_id());
    auto* iw = interface_cast<IArrayWidget>(obj);
    ASSERT_NE(iw, nullptr);

    auto presets = iw->presets();
    EXPECT_EQ(presets.set_at(1, 99.f), ReturnValue::Success);
    EXPECT_FLOAT_EQ(presets.at(1), 99.f);

    // Other elements unchanged
    EXPECT_FLOAT_EQ(presets.at(0), 1.f);
    EXPECT_FLOAT_EQ(presets.at(2), 3.f);
}

TEST_F(ArrayPropertyTest, EraseAt)
{
    auto obj = instance().create<IObject>(ArrayWidget::class_id());
    auto* iw = interface_cast<IArrayWidget>(obj);
    ASSERT_NE(iw, nullptr);

    auto presets = iw->presets();
    EXPECT_EQ(presets.erase_at(1), ReturnValue::Success);
    EXPECT_EQ(presets.size(), 2u);
    EXPECT_FLOAT_EQ(presets.at(0), 1.f);
    EXPECT_FLOAT_EQ(presets.at(1), 3.f);
}

TEST_F(ArrayPropertyTest, Clear)
{
    auto obj = instance().create<IObject>(ArrayWidget::class_id());
    auto* iw = interface_cast<IArrayWidget>(obj);
    ASSERT_NE(iw, nullptr);

    auto presets = iw->presets();
    EXPECT_EQ(presets.size(), 3u);
    presets.clear();
    EXPECT_EQ(presets.size(), 0u);
    EXPECT_TRUE(presets.empty());
}

// ReadOnly array property

TEST_F(ArrayPropertyTest, ReadOnlyDefault)
{
    auto obj = instance().create<IObject>(ArrayWidget::class_id());
    auto* iw = interface_cast<IArrayWidget>(obj);
    ASSERT_NE(iw, nullptr);

    auto ids = iw->ids();
    EXPECT_TRUE(ids);
    EXPECT_EQ(ids.size(), 2u);
    EXPECT_EQ(ids.at(0), 10);
    EXPECT_EQ(ids.at(1), 20);
}

TEST_F(ArrayPropertyTest, ReadOnlyRejectsSetAt)
{
    auto obj = instance().create<IObject>(ArrayWidget::class_id());
    auto* iw = interface_cast<IArrayWidget>(obj);
    ASSERT_NE(iw, nullptr);

    // Get the IArrayProperty interface directly
    auto* meta = interface_cast<IMetadata>(obj);
    ASSERT_NE(meta, nullptr);
    auto prop = meta->get_property("ids");
    ASSERT_TRUE(prop);

    auto* ap = interface_cast<IArrayProperty>(prop);
    ASSERT_NE(ap, nullptr);

    Any<int32_t> val(99);
    EXPECT_EQ(ap->set_at(0, val), ReturnValue::ReadOnly);
    EXPECT_EQ(ap->push_back(val), ReturnValue::ReadOnly);
    EXPECT_EQ(ap->erase_at(0), ReturnValue::ReadOnly);
}

// Bounds checking

TEST_F(ArrayPropertyTest, GetAtOutOfBounds)
{
    auto obj = instance().create<IObject>(ArrayWidget::class_id());
    auto* iw = interface_cast<IArrayWidget>(obj);
    ASSERT_NE(iw, nullptr);

    auto items = iw->items();
    // Empty array, any index is out of bounds
    EXPECT_FLOAT_EQ(items.at(0), 0.f); // returns default on failure
}

TEST_F(ArrayPropertyTest, SetAtOutOfBounds)
{
    auto obj = instance().create<IObject>(ArrayWidget::class_id());
    auto* meta = interface_cast<IMetadata>(obj);
    ASSERT_NE(meta, nullptr);
    auto prop = meta->get_property("items");
    auto* ap = interface_cast<IArrayProperty>(prop);
    ASSERT_NE(ap, nullptr);

    Any<float> val(1.f);
    EXPECT_EQ(ap->set_at(0, val), ReturnValue::InvalidArgument);
}

TEST_F(ArrayPropertyTest, EraseAtOutOfBounds)
{
    auto obj = instance().create<IObject>(ArrayWidget::class_id());
    auto* meta = interface_cast<IMetadata>(obj);
    ASSERT_NE(meta, nullptr);
    auto prop = meta->get_property("items");
    auto* ap = interface_cast<IArrayProperty>(prop);
    ASSERT_NE(ap, nullptr);

    EXPECT_EQ(ap->erase_at(0), ReturnValue::InvalidArgument);
}

// State struct integration

TEST_F(ArrayPropertyTest, StateStructAccess)
{
    auto obj = instance().create<IObject>(ArrayWidget::class_id());
    auto* iw = interface_cast<IArrayWidget>(obj);
    auto* ps = interface_cast<IPropertyState>(obj);
    ASSERT_NE(iw, nullptr);
    ASSERT_NE(ps, nullptr);

    auto* state = ps->get_property_state<IArrayWidget>();
    ASSERT_NE(state, nullptr);

    // Default in state struct
    EXPECT_EQ(state->presets.size(), 3u);
    EXPECT_FLOAT_EQ(state->presets[0], 1.f);

    // Modify through property, read from state
    auto presets = iw->presets();
    presets.push_back(4.f);
    EXPECT_EQ(state->presets.size(), 4u);
    EXPECT_FLOAT_EQ(state->presets[3], 4.f);

    // Modify state directly, read through property
    state->presets[0] = 100.f;
    EXPECT_FLOAT_EQ(presets.at(0), 100.f);
}

// get_value (full vector copy)

TEST_F(ArrayPropertyTest, GetValueReturnsCopy)
{
    auto obj = instance().create<IObject>(ArrayWidget::class_id());
    auto* iw = interface_cast<IArrayWidget>(obj);
    ASSERT_NE(iw, nullptr);

    auto presets = iw->presets();
    auto values = presets.get_value();
    EXPECT_EQ(values.size(), 3u);
    EXPECT_FLOAT_EQ(values[0], 1.f);
    EXPECT_FLOAT_EQ(values[1], 2.f);
    EXPECT_FLOAT_EQ(values[2], 3.f);
}

// on_changed fires

TEST_F(ArrayPropertyTest, OnChangedFiresOnPushBack)
{
    auto obj = instance().create<IObject>(ArrayWidget::class_id());
    auto* iw = interface_cast<IArrayWidget>(obj);
    ASSERT_NE(iw, nullptr);

    auto items = iw->items();
    int callCount = 0;
    Callback handler([&](FnArgs) -> ReturnValue {
        callCount++;
        return ReturnValue::Success;
    });
    items.add_on_changed(handler);
    callCount = 0; // reset after initial subscribe fire

    items.push_back(1.f);
    EXPECT_EQ(callCount, 1);

    items.push_back(2.f);
    EXPECT_EQ(callCount, 2);
}

TEST_F(ArrayPropertyTest, OnChangedFiresOnSetAt)
{
    auto obj = instance().create<IObject>(ArrayWidget::class_id());
    auto* iw = interface_cast<IArrayWidget>(obj);
    ASSERT_NE(iw, nullptr);

    auto presets = iw->presets();
    int callCount = 0;
    Callback handler([&](FnArgs) -> ReturnValue {
        callCount++;
        return ReturnValue::Success;
    });
    presets.add_on_changed(handler);
    callCount = 0;

    presets.set_at(0, 99.f);
    EXPECT_EQ(callCount, 1);
}

// MemberKind::ArrayProperty in static metadata

TEST_F(ArrayPropertyTest, StaticMetadataKind)
{
    auto* info = instance().type_registry().get_class_info(ArrayWidget::class_id());
    ASSERT_NE(info, nullptr);

    // items, presets, ids are ArrayProperty; count is Property
    int arrCount = 0;
    int propCount = 0;
    for (auto& m : info->members) {
        if (m.kind == MemberKind::ArrayProperty) {
            arrCount++;
        } else if (m.kind == MemberKind::Property) {
            propCount++;
        }
    }
    EXPECT_EQ(arrCount, 3);
    EXPECT_EQ(propCount, 1);
}

// Owning ArrayAny constructors

TEST_F(ArrayPropertyTest, OwningDefaultConstructor)
{
    ArrayAny<float> a;
    EXPECT_TRUE(a);
    EXPECT_EQ(a.size(), 0u);
    EXPECT_TRUE(a.empty());
}

TEST_F(ArrayPropertyTest, OwningInitializerList)
{
    ArrayAny<float> a{1.f, 2.f, 3.f};
    EXPECT_TRUE(a);
    EXPECT_EQ(a.size(), 3u);
    EXPECT_FLOAT_EQ(a.at(0), 1.f);
    EXPECT_FLOAT_EQ(a.at(1), 2.f);
    EXPECT_FLOAT_EQ(a.at(2), 3.f);
}

TEST_F(ArrayPropertyTest, OwningFromArrayView)
{
    float data[] = {10.f, 20.f, 30.f, 40.f};
    array_view<float> view(data, 4);
    ArrayAny<float> a(view);
    EXPECT_TRUE(a);
    EXPECT_EQ(a.size(), 4u);
    EXPECT_FLOAT_EQ(a.at(0), 10.f);
    EXPECT_FLOAT_EQ(a.at(1), 20.f);
    EXPECT_FLOAT_EQ(a.at(2), 30.f);
    EXPECT_FLOAT_EQ(a.at(3), 40.f);
}

TEST_F(ArrayPropertyTest, OwningMutation)
{
    ArrayAny<float> a{1.f, 2.f};
    EXPECT_EQ(a.push_back(3.f), ReturnValue::Success);
    EXPECT_EQ(a.size(), 3u);
    EXPECT_FLOAT_EQ(a.at(2), 3.f);

    EXPECT_EQ(a.set_at(0, 99.f), ReturnValue::Success);
    EXPECT_FLOAT_EQ(a.at(0), 99.f);

    EXPECT_EQ(a.erase_at(1), ReturnValue::Success);
    EXPECT_EQ(a.size(), 2u);

    a.clear();
    EXPECT_EQ(a.size(), 0u);
}

TEST_F(ArrayPropertyTest, OwningGetSetValue)
{
    ArrayAny<float> a{1.f, 2.f};
    auto v = a.get_value();
    EXPECT_EQ(v.size(), 2u);
    EXPECT_FLOAT_EQ(v[0], 1.f);
    EXPECT_FLOAT_EQ(v[1], 2.f);

    vector<float> newVec;
    newVec.push_back(5.f);
    newVec.push_back(6.f);
    newVec.push_back(7.f);
    EXPECT_EQ(a.set_value(newVec), ReturnValue::Success);
    EXPECT_EQ(a.size(), 3u);
    EXPECT_FLOAT_EQ(a.at(0), 5.f);
}

// Mixed interface: regular properties still work alongside array properties

TEST_F(ArrayPropertyTest, MixedPropertiesWork)
{
    auto obj = instance().create<IObject>(ArrayWidget::class_id());
    auto* iw = interface_cast<IArrayWidget>(obj);
    ASSERT_NE(iw, nullptr);

    // Regular property works
    iw->count().set_value(42);
    EXPECT_EQ(iw->count().get_value(), 42);

    // Array property works alongside
    auto items = iw->items();
    items.push_back(1.f);
    EXPECT_EQ(items.size(), 1u);
}
