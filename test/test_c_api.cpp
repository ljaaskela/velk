#include <velk_c.h>

#include <velk/api/velk.h>
#include <velk/ext/object.h>
#include <velk/interface/intf_metadata.h>

#include <gtest/gtest.h>

// Test interface: simple widget with a float and an event
class ICApiWidget : public velk::Interface<ICApiWidget>
{
public:
    VELK_INTERFACE(
        (PROP, float, width, 100.f),
        (PROP, int, count, 0),
        (EVT, on_clicked)
    )
};

class CApiWidget : public velk::ext::Object<CApiWidget, ICApiWidget>
{
};

class CApi : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        ::velk::register_type<CApiWidget>(::velk::instance());
    }
};

// UID helpers

TEST_F(CApi, UidFromString)
{
    velk_uid uid = velk_uid_from_string("a66badbf-c750-4580-b035-b5446806d67e");
    EXPECT_NE(uid.hi, uint64_t(0));
    EXPECT_NE(uid.lo, uint64_t(0));

    // Zero UID from null
    velk_uid zero = velk_uid_from_string(nullptr);
    EXPECT_EQ(zero.hi, uint64_t(0));
    EXPECT_EQ(zero.lo, uint64_t(0));
}

// Lifecycle

TEST_F(CApi, CreateAndRelease)
{
    velk_uid class_id = velk_uid_from_string(nullptr);
    // Get the class UID via C++ and convert
    auto cpp_uid = CApiWidget::class_id();
    class_id.hi = cpp_uid.hi;
    class_id.lo = cpp_uid.lo;

    velk_object obj = velk_create(class_id, 0);
    ASSERT_NE(obj, nullptr);

    velk_release(obj);
}

// Object info

TEST_F(CApi, ClassUidAndName)
{
    auto cpp_uid = CApiWidget::class_id();
    velk_uid class_id = {cpp_uid.hi, cpp_uid.lo};

    velk_object obj = velk_create(class_id, 0);
    ASSERT_NE(obj, nullptr);

    velk_uid uid = velk_class_uid(obj);
    EXPECT_EQ(uid.hi, cpp_uid.hi);
    EXPECT_EQ(uid.lo, cpp_uid.lo);

    const char* name = velk_class_name(obj);
    ASSERT_NE(name, nullptr);
    EXPECT_NE(strlen(name), size_t(0));

    velk_release(obj);
}

// Property get/set

TEST_F(CApi, PropertyGetSetFloat)
{
    auto cpp_uid = CApiWidget::class_id();
    velk_uid class_id = {cpp_uid.hi, cpp_uid.lo};

    velk_object obj = velk_create(class_id, 0);
    ASSERT_NE(obj, nullptr);

    velk_property prop = velk_get_property(obj, "width");
    ASSERT_NE(prop, nullptr);

    // Read default value
    float val = 0.f;
    velk_result r = velk_property_get_float(prop, &val);
    EXPECT_GE(r, 0);
    EXPECT_FLOAT_EQ(val, 100.f);

    // Set new value
    r = velk_property_set_float(prop, 42.5f);
    EXPECT_GE(r, 0);

    // Read back
    val = 0.f;
    r = velk_property_get_float(prop, &val);
    EXPECT_GE(r, 0);
    EXPECT_FLOAT_EQ(val, 42.5f);

    velk_release(prop);
    velk_release(obj);
}

TEST_F(CApi, PropertyGetSetInt32)
{
    auto cpp_uid = CApiWidget::class_id();
    velk_uid class_id = {cpp_uid.hi, cpp_uid.lo};

    velk_object obj = velk_create(class_id, 0);
    ASSERT_NE(obj, nullptr);

    velk_property prop = velk_get_property(obj, "count");
    ASSERT_NE(prop, nullptr);

    int32_t val = -1;
    velk_result r = velk_property_get_int32(prop, &val);
    EXPECT_GE(r, 0);
    EXPECT_EQ(val, 0);

    r = velk_property_set_int32(prop, 99);
    EXPECT_GE(r, 0);

    val = -1;
    r = velk_property_get_int32(prop, &val);
    EXPECT_GE(r, 0);
    EXPECT_EQ(val, 99);

    velk_release(prop);
    velk_release(obj);
}

// Event + callback

static void test_callback(void* user_data, velk_property source)
{
    (void)source;
    int* counter = static_cast<int*>(user_data);
    (*counter)++;
}

TEST_F(CApi, EventAddRemoveCallback)
{
    auto cpp_uid = CApiWidget::class_id();
    velk_uid class_id = {cpp_uid.hi, cpp_uid.lo};

    velk_object obj = velk_create(class_id, 0);
    ASSERT_NE(obj, nullptr);

    // Get the on_changed event of the "width" property
    velk_property prop = velk_get_property(obj, "width");
    ASSERT_NE(prop, nullptr);

    // Get the on_clicked event from the object
    velk_event evt = velk_get_event(obj, "on_clicked");
    ASSERT_NE(evt, nullptr);

    int counter = 0;
    velk_function handler = velk_create_callback(&test_callback, &counter);
    ASSERT_NE(handler, nullptr);

    velk_result r = velk_event_add(evt, handler);
    EXPECT_GE(r, 0);

    // Invoke the event via C++ to trigger the callback
    auto* raw_evt = reinterpret_cast<velk::IEvent*>(evt);
    raw_evt->invoke({});
    EXPECT_EQ(counter, 1);

    // Remove the handler
    r = velk_event_remove(evt, handler);
    EXPECT_GE(r, 0);

    // Invoke again: should not fire
    raw_evt->invoke({});
    EXPECT_EQ(counter, 1);

    velk_release(handler);
    velk_release(evt);
    velk_release(prop);
    velk_release(obj);
}

// Update loop (smoke test)

TEST_F(CApi, UpdateDoesNotCrash)
{
    velk_update(0);
    velk_update(16000); // 16ms
}

// Null safety

TEST_F(CApi, NullHandlesReturnGracefully)
{
    velk_release(velk_interface(nullptr));
    velk_acquire(velk_interface(nullptr));

    EXPECT_EQ(velk_cast(velk_interface(nullptr), {0, 0}), nullptr);
    EXPECT_EQ(velk_get_property(nullptr, "x"), nullptr);
    EXPECT_EQ(velk_get_event(nullptr, "x"), nullptr);

    velk_uid uid = velk_class_uid(nullptr);
    EXPECT_EQ(uid.hi, uint64_t(0));
    EXPECT_EQ(uid.lo, uint64_t(0));

    EXPECT_EQ(velk_class_name(nullptr), nullptr);
}

// Type UID constants

TEST_F(CApi, TypeUidConstantsNonZero)
{
    EXPECT_NE(VELK_TYPE_FLOAT.hi | VELK_TYPE_FLOAT.lo, uint64_t(0));
    EXPECT_NE(VELK_TYPE_INT32.hi | VELK_TYPE_INT32.lo, uint64_t(0));
    EXPECT_NE(VELK_TYPE_DOUBLE.hi | VELK_TYPE_DOUBLE.lo, uint64_t(0));
    EXPECT_NE(VELK_TYPE_BOOL.hi | VELK_TYPE_BOOL.lo, uint64_t(0));
}
