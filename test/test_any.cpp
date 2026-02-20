#include <gtest/gtest.h>

#include <velk/api/any.h>
#include <velk/api/velk.h>
#include <velk/ext/any.h>
#include <velk/interface/intf_any.h>

using namespace velk;

TEST(Any, FloatConstructAndGetValue)
{
    Any<float> a(3.14f);
    EXPECT_FLOAT_EQ(a.get_value(), 3.14f);
}

TEST(Any, FloatSetAndGetValue)
{
    Any<float> a;
    a.set_value(42.f);
    EXPECT_FLOAT_EQ(a.get_value(), 42.f);
}

TEST(Any, DefaultConstructedHasZeroValue)
{
    Any<int> a;
    EXPECT_EQ(a.get_value(), 0);
}

TEST(Any, CopySemanticsShareUnderlying)
{
    Any<float> a(10.f);
    Any<float> b = a;
    EXPECT_FLOAT_EQ(b.get_value(), 10.f);

    // Both point to the same IAny, so setting through one reflects in the other
    a.set_value(20.f);
    EXPECT_FLOAT_EQ(b.get_value(), 20.f);
}

TEST(Any, ConstAnyReadOnly)
{
    Any<float> a(5.f);
    const IAny* raw = a.get_any_interface();
    Any<const float> ca(raw);
    EXPECT_TRUE(ca);
    EXPECT_FLOAT_EQ(ca.get_value(), 5.f);
}

TEST(AnyValue, CloneProducesIndependentCopy)
{
    ext::AnyValue<float> original;
    original.set_value(99.f);
    auto cloned = original.clone();
    ASSERT_NE(cloned, nullptr);

    // Modify original — clone should be unaffected
    original.set_value(0.f);
    float clonedVal{};
    cloned->get_data(&clonedVal, sizeof(float), type_uid<float>());
    EXPECT_FLOAT_EQ(clonedVal, 99.f);
}

TEST(AnyRef, ReadWriteThroughExternalPointer)
{
    float storage = 100.f;
    ext::AnyRef<float> ref(&storage);

    EXPECT_FLOAT_EQ(ref.get_value(), 100.f);

    ref.set_value(200.f);
    EXPECT_FLOAT_EQ(storage, 200.f);

    storage = 300.f;
    EXPECT_FLOAT_EQ(ref.get_value(), 300.f);
}

TEST(AnyRef, CloneIsIndependent)
{
    float storage = 42.f;
    ext::AnyRef<float> ref(&storage);
    auto cloned = ref.clone();
    ASSERT_NE(cloned, nullptr);

    storage = 0.f;
    float clonedVal{};
    cloned->get_data(&clonedVal, sizeof(float), type_uid<float>());
    EXPECT_FLOAT_EQ(clonedVal, 42.f);
}

TEST(AnyValue, SetSameValueReturnsNothingToDo)
{
    ext::AnyValue<int> a;
    a.set_value(5);
    EXPECT_EQ(a.set_value(5), ReturnValue::NOTHING_TO_DO);
    EXPECT_EQ(a.set_value(6), ReturnValue::SUCCESS);
}

TEST(Any, TypeMismatchProducesInvalidWrapper)
{
    Any<float> a(1.f);
    const IAny* raw = a.get_any_interface();
    // Try to wrap as int — should produce an invalid (null) Any
    Any<const int> bad(raw);
    EXPECT_FALSE(bad);
}

TEST(AnyValue, CopyFromCompatible)
{
    ext::AnyValue<int> a;
    a.set_value(42);
    ext::AnyValue<int> b;
    EXPECT_EQ(b.copy_from(a), ReturnValue::SUCCESS);
    EXPECT_EQ(b.get_value(), 42);
}

TEST(AnyValue, CopyFromIncompatibleFails)
{
    ext::AnyValue<int> a;
    a.set_value(42);
    ext::AnyValue<float> b;
    EXPECT_EQ(b.copy_from(a), ReturnValue::FAIL);
}
