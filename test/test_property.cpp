#include <gtest/gtest.h>

#include <velk/api/any.h>
#include <velk/api/callback.h>
#include <velk/api/property.h>
#include <velk/api/velk.h>
#include <velk/interface/intf_property.h>

using namespace velk;

// ReadWrite property

TEST(Property, DefaultConstructedHasInitialValue)
{
    auto p = create_property<float>();
    EXPECT_FLOAT_EQ(p.get_value(), 0.f);
}

TEST(Property, ConstructWithValue)
{
    auto p = create_property<int>(42);
    EXPECT_EQ(p.get_value(), 42);
}

TEST(Property, SetGetRoundTrip)
{
    auto p = create_property<float>();
    p.set_value(3.14f);
    EXPECT_FLOAT_EQ(p.get_value(), 3.14f);
}

TEST(Property, CopySemanticsShareSameIProperty)
{
    auto p = create_property<float>();
    p.set_value(10.f);
    Property<float> copy = p;
    EXPECT_FLOAT_EQ(copy.get_value(), 10.f);

    // Both copies reference the same IProperty
    p.set_value(20.f);
    EXPECT_FLOAT_EQ(copy.get_value(), 20.f);
}

TEST(Property, OnChangedEventFires)
{
    int callCount = 0;
    float receivedValue = 0.f;

    auto p = create_property<float>();

    Callback handler([&](FnArgs args) -> ReturnValue {
        callCount++;
        if (auto v = Any<const float>(args[0])) {
            receivedValue = v.get_value();
        }
        return ReturnValue::SUCCESS;
    });

    p.add_on_changed(handler);
    p.set_value(42.f);

    EXPECT_EQ(callCount, 1);
    EXPECT_FLOAT_EQ(receivedValue, 42.f);
}

TEST(Property, SetSameValueReturnsNothingToDo)
{
    auto p = create_property<int>(5);

    auto iprop = p.get_property_interface();
    ASSERT_TRUE(iprop);

    Any<int> val(5);
    auto result = iprop->set_value(val);
    EXPECT_EQ(result, ReturnValue::NOTHING_TO_DO);
}

TEST(Property, SetDifferentValueReturnsSuccess)
{
    auto p = create_property<int>(5);
    ASSERT_TRUE(p);

    auto result = p.set_value(10);
    EXPECT_EQ(result, ReturnValue::SUCCESS);
    EXPECT_EQ(p.get_value(), 10);
}

TEST(Property, OnChangedDoesNotFireOnSameValue)
{
    int callCount = 0;

    auto p = create_property<int>(5);

    Callback handler([&](FnArgs) -> ReturnValue {
        callCount++;
        return ReturnValue::SUCCESS;
    });

    p.add_on_changed(handler);
    EXPECT_EQ(p.set_value(5), ReturnValue::NOTHING_TO_DO); // same value
    EXPECT_EQ(callCount, 0);
}

TEST(Property, BoolConversion)
{
    auto p = create_property<float>();
    EXPECT_TRUE(p);
}

// ReadOnly property

TEST(Property, DefaultConstructedReadOnlyHasInitialValue)
{
    auto p = create_property<const float>();
    EXPECT_FLOAT_EQ(p.get_value(), 0.f);
}

TEST(Property, ConstructReadOnlyWithValue)
{
    auto p = create_property<const int>(42);
    EXPECT_EQ(p.get_value(), 42);
}

TEST(Property, ConstructReadOnlySetFails)
{
    auto p = create_property<const int>(42);
    EXPECT_EQ(p.get_value(), 42);

    // Can still create a "ReadWrite" Property<T> using the interface, but writes should still fail.
    Property<int> pp(p.get_property_interface());
    EXPECT_TRUE(pp);
    EXPECT_EQ(pp.set_value(1), ReturnValue::READ_ONLY);
}
