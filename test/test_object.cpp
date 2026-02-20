#include <gtest/gtest.h>

#include <velk/api/any.h>
#include <velk/api/callback.h>
#include <velk/api/function_context.h>
#include <velk/api/property.h>
#include <velk/api/velk.h>
#include <velk/ext/object.h>
#include <velk/interface/intf_event.h>
#include <velk/interface/intf_metadata.h>
#include <velk/interface/intf_property.h>
#include <velk/interface/types.h>

using namespace velk;

// --- Test interfaces and implementation ---

class ITestWidget : public Interface<ITestWidget>
{
public:
    VELK_INTERFACE(
        (PROP, float, width, 100.f),
        (PROP, float, height, 50.f),
        (RPROP, int, id, 42),
        (EVT, on_clicked),
        (FN, void, reset)
    )
};

class ITestSerializable : public Interface<ITestSerializable>
{
public:
    VELK_INTERFACE(
        (PROP, int, version, 1),
        (FN, void, serialize)
    )
};

class ITestMath : public Interface<ITestMath>
{
public:
    VELK_INTERFACE(
        (FN, int, add, (int, x), (int, y))
    )
};

class ITestRaw : public Interface<ITestRaw>
{
public:
    VELK_INTERFACE(
        (FN_RAW, process)
    )
};

class TestWidget : public ext::Object<TestWidget, ITestWidget, ITestSerializable, ITestMath, ITestRaw>
{
public:
    int resetCallCount = 0;
    int serializeCallCount = 0;
    int lastAddResult = 0;
    int processCallCount = 0;

    void fn_reset() override
    {
        resetCallCount++;
    }

    void fn_serialize() override
    {
        serializeCallCount++;
    }

    int fn_add(int x, int y) override
    {
        lastAddResult = x + y;
        return lastAddResult;
    }

    IAny::Ptr fn_process(FnArgs) override
    {
        processCallCount++;
        return nullptr;
    }
};

// --- Fixture to register once ---

class ObjectTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        instance().type_registry().register_type<TestWidget>();
    }
};

// --- Tests ---

TEST_F(ObjectTest, RegisterAndCreate)
{
    auto obj = instance().create<IObject>(TestWidget::class_id());
    ASSERT_TRUE(obj);
}

TEST_F(ObjectTest, InterfaceCastSucceeds)
{
    auto obj = instance().create<IObject>(TestWidget::class_id());
    ASSERT_TRUE(obj);

    auto* iw = interface_cast<ITestWidget>(obj);
    EXPECT_NE(iw, nullptr);

    auto* is = interface_cast<ITestSerializable>(obj);
    EXPECT_NE(is, nullptr);

    auto* meta = interface_cast<IMetadata>(obj);
    EXPECT_NE(meta, nullptr);
}

TEST_F(ObjectTest, InterfaceCastFailsForNonImplemented)
{
    // IVelk is not implemented by TestWidget
    auto obj = instance().create<IObject>(TestWidget::class_id());
    ASSERT_TRUE(obj);

    auto* bad = interface_cast<IVelk>(obj);
    EXPECT_EQ(bad, nullptr);
}

TEST_F(ObjectTest, MetadataGetPropertyByName)
{
    auto obj = instance().create<IObject>(TestWidget::class_id());
    auto* meta = interface_cast<IMetadata>(obj);
    ASSERT_NE(meta, nullptr);

    auto widthProp = meta->get_property("width");
    EXPECT_TRUE(widthProp);

    auto heightProp = meta->get_property("height");
    EXPECT_TRUE(heightProp);

    auto versionProp = meta->get_property("version");
    EXPECT_TRUE(versionProp);
}

TEST_F(ObjectTest, MetadataGetPropertyReturnsNullForUnknown)
{
    auto obj = instance().create<IObject>(TestWidget::class_id());
    auto* meta = interface_cast<IMetadata>(obj);
    ASSERT_NE(meta, nullptr);

    auto bogus = meta->get_property("nonexistent");
    EXPECT_FALSE(bogus);
}

TEST_F(ObjectTest, MetadataGetEventByName)
{
    auto obj = instance().create<IObject>(TestWidget::class_id());
    auto* meta = interface_cast<IMetadata>(obj);
    ASSERT_NE(meta, nullptr);

    auto event = meta->get_event("on_clicked");
    EXPECT_TRUE(event);
}

TEST_F(ObjectTest, MetadataGetFunctionByName)
{
    auto obj = instance().create<IObject>(TestWidget::class_id());
    auto* meta = interface_cast<IMetadata>(obj);
    ASSERT_NE(meta, nullptr);

    auto fn = meta->get_function("reset");
    EXPECT_TRUE(fn);

    auto fn2 = meta->get_function("serialize");
    EXPECT_TRUE(fn2);
}

TEST_F(ObjectTest, PropertyDefaultsFromInterface)
{
    auto obj = instance().create<IObject>(TestWidget::class_id());
    auto* iw = interface_cast<ITestWidget>(obj);
    ASSERT_NE(iw, nullptr);

    EXPECT_FLOAT_EQ(iw->width().get_value(), 100.f);
    EXPECT_FLOAT_EQ(iw->height().get_value(), 50.f);

    auto* is = interface_cast<ITestSerializable>(obj);
    ASSERT_NE(is, nullptr);
    EXPECT_EQ(is->version().get_value(), 1);
}

TEST_F(ObjectTest, PropertySetAndGet)
{
    auto obj = instance().create<IObject>(TestWidget::class_id());
    auto* iw = interface_cast<ITestWidget>(obj);
    ASSERT_NE(iw, nullptr);

    iw->width().set_value(42.f);
    EXPECT_FLOAT_EQ(iw->width().get_value(), 42.f);
}

TEST_F(ObjectTest, FunctionInvokeViaInterface)
{
    auto obj = instance().create<IObject>(TestWidget::class_id());
    auto* iw = interface_cast<ITestWidget>(obj);
    ASSERT_NE(iw, nullptr);

    invoke_function(iw->reset());
    // Verify the virtual fn_reset was called (via the raw pointer)
    auto* raw = static_cast<TestWidget*>(interface_cast<ITestWidget>(obj));
    ASSERT_NE(raw, nullptr);
    EXPECT_EQ(raw->resetCallCount, 1);
}

TEST_F(ObjectTest, FunctionInvokeByName)
{
    auto obj = instance().create<IObject>(TestWidget::class_id());
    invoke_function(obj.get(), "reset");

    auto* raw = static_cast<TestWidget*>(interface_cast<ITestWidget>(obj));
    ASSERT_NE(raw, nullptr);
    EXPECT_EQ(raw->resetCallCount, 1);
}

TEST_F(ObjectTest, StaticMetadataViaGetClassInfo)
{
    auto* info = instance().type_registry().get_class_info(TestWidget::class_id());
    ASSERT_NE(info, nullptr);

    // ITestWidget: width, height, id, on_clicked, reset
    // ITestSerializable: version, serialize
    // ITestMath: add
    // ITestRaw: process
    EXPECT_EQ(info->members.size(), 9u);

    // Check first member
    EXPECT_EQ(info->members[0].name, "width");
    EXPECT_EQ(info->members[0].kind, MemberKind::Property);

    EXPECT_EQ(info->members[2].name, "id");
    EXPECT_EQ(info->members[2].kind, MemberKind::Property);

    EXPECT_EQ(info->members[3].name, "on_clicked");
    EXPECT_EQ(info->members[3].kind, MemberKind::Event);

    EXPECT_EQ(info->members[4].name, "reset");
    EXPECT_EQ(info->members[4].kind, MemberKind::Function);
}

TEST_F(ObjectTest, InterfaceListViaGetClassInfo)
{
    auto* info = instance().type_registry().get_class_info(TestWidget::class_id());
    ASSERT_NE(info, nullptr);

    // Object<TestWidget, ITestWidget, ITestSerializable, ITestMath, ITestRaw>
    // Pack is {IObject, IMetadataContainer, ITestWidget, ITestSerializable, ITestMath, ITestRaw}
    // Chain walks: IObject, IMetadataContainer->IMetadata->IPropertyState,
    //             ITestWidget, ITestSerializable, ITestMath, ITestRaw (IObject deduplicated)
    EXPECT_EQ(info->interfaces.size(), 8u);

    EXPECT_EQ(info->interfaces[0].uid, IObject::UID);
    EXPECT_EQ(info->interfaces[1].uid, IMetadataContainer::UID);
    EXPECT_EQ(info->interfaces[2].uid, IMetadata::UID);
    EXPECT_EQ(info->interfaces[3].uid, IPropertyState::UID);
    EXPECT_EQ(info->interfaces[4].uid, ITestWidget::UID);
    EXPECT_EQ(info->interfaces[5].uid, ITestSerializable::UID);
    EXPECT_EQ(info->interfaces[6].uid, ITestMath::UID);
    EXPECT_EQ(info->interfaces[7].uid, ITestRaw::UID);
}

TEST_F(ObjectTest, StaticDefaultValues)
{
    auto* info = instance().type_registry().get_class_info(TestWidget::class_id());
    ASSERT_NE(info, nullptr);

    EXPECT_FLOAT_EQ(get_default_value<float>(info->members[0]), 100.f);  // width
    EXPECT_FLOAT_EQ(get_default_value<float>(info->members[1]), 50.f);   // height
    EXPECT_EQ(get_default_value<int>(info->members[2]), 42);             // id (RPROP)
    EXPECT_EQ(get_default_value<int>(info->members[5]), 1);              // version
}

TEST_F(ObjectTest, PropertyStateReadWrite)
{
    auto obj = instance().create<IObject>(TestWidget::class_id());
    auto* iw = interface_cast<ITestWidget>(obj);
    auto* ps = interface_cast<IPropertyState>(obj);
    ASSERT_NE(iw, nullptr);
    ASSERT_NE(ps, nullptr);

    auto* state = ps->get_property_state<ITestWidget>();
    auto *state2 = get_property_state<ITestWidget>(ps);
    ASSERT_NE(state, nullptr);
    ASSERT_EQ(state, state2);

    // Defaults in state struct
    EXPECT_FLOAT_EQ(state->width, 100.f);
    EXPECT_FLOAT_EQ(state->height, 50.f);

    // Write through property, read from state
    iw->width().set_value(200.f);
    EXPECT_FLOAT_EQ(state->width, 200.f);

    // Write to state directly, read through property
    state->height = 75.f;
    EXPECT_FLOAT_EQ(iw->height().get_value(), 75.f);
}

TEST_F(ObjectTest, TypedFunctionInvoke)
{
    auto obj = instance().create<IObject>(TestWidget::class_id());
    auto* raw = static_cast<TestWidget*>(interface_cast<ITestWidget>(obj));
    ASSERT_NE(raw, nullptr);

    // Invoke typed function via IAny args
    Any<int> a(3);
    Any<int> b(7);
    const IAny* ptrs[] = {a, b};
    FnArgs args{ptrs, 2};
    invoke_function(obj.get(), "add", args);
    EXPECT_EQ(raw->lastAddResult, 10);

    // Invoke via variadic helper
    invoke_function(obj.get(), "add", Any<int>(10), Any<int>(20));
    EXPECT_EQ(raw->lastAddResult, 30);
}

TEST_F(ObjectTest, TypedFunctionReturnValue)
{
    auto obj = instance().create<IObject>(TestWidget::class_id());

    // Invoke typed function that returns int
    auto result = invoke_function(obj.get(), "add", Any<int>(3), Any<int>(7));
    ASSERT_TRUE(result);

    // The result should be an IAny containing the sum
    int value = 0;
    result->get_data(&value, sizeof(int), type_uid<int>());
    EXPECT_EQ(value, 10);
}

TEST_F(ObjectTest, VoidFunctionReturnsNull)
{
    auto obj = instance().create<IObject>(TestWidget::class_id());

    // Invoke void function — should return nullptr
    auto result = invoke_function(obj.get(), "reset");
    EXPECT_FALSE(result);
}

TEST_F(ObjectTest, TypedFunctionArgMetadata)
{
    auto* info = instance().type_registry().get_class_info(TestWidget::class_id());
    ASSERT_NE(info, nullptr);

    // Find the "add" member
    const MemberDesc* addDesc = nullptr;
    for (auto& m : info->members) {
        if (m.name == "add") { addDesc = &m; break; }
    }
    ASSERT_NE(addDesc, nullptr);
    EXPECT_EQ(addDesc->kind, MemberKind::Function);

    auto* fk = addDesc->functionKind();
    ASSERT_NE(fk, nullptr);
    EXPECT_EQ(fk->args.size(), 2u);
    EXPECT_EQ(fk->args[0].name, "x");
    EXPECT_EQ(fk->args[0].typeUid, type_uid<int>());
    EXPECT_EQ(fk->args[1].name, "y");
    EXPECT_EQ(fk->args[1].typeUid, type_uid<int>());
}

TEST_F(ObjectTest, ZeroArgFunctionHasNoArgMetadata)
{
    auto* info = instance().type_registry().get_class_info(TestWidget::class_id());
    ASSERT_NE(info, nullptr);

    const MemberDesc* resetDesc = nullptr;
    for (auto& m : info->members) {
        if (m.name == "reset") { resetDesc = &m; break; }
    }
    ASSERT_NE(resetDesc, nullptr);

    auto* fk = resetDesc->functionKind();
    ASSERT_NE(fk, nullptr);
    EXPECT_TRUE(fk->args.empty());
}

TEST_F(ObjectTest, FnRawInvoke)
{
    auto obj = instance().create<IObject>(TestWidget::class_id());
    auto* raw = static_cast<TestWidget*>(interface_cast<ITestWidget>(obj));
    ASSERT_NE(raw, nullptr);

    invoke_function(obj.get(), "process");
    EXPECT_EQ(raw->processCallCount, 1);
}

TEST_F(ObjectTest, PropBindGetDefault)
{
    // PropBind for ITestWidget::State::width should return 100.f
    using Bind = detail::PropBind<ITestWidget::State, &ITestWidget::State::width>;
    const IAny* def = Bind::getDefault();
    ASSERT_NE(def, nullptr);
    float val = 0.f;
    def->get_data(&val, sizeof(float), type_uid<float>());
    EXPECT_FLOAT_EQ(val, 100.f);
}

TEST_F(ObjectTest, PropBindCreateRef)
{
    // PropBind::createRef should create an AnyRef pointing into a State struct
    ITestWidget::State state;
    state.width = 42.f;
    auto ref = detail::PropBind<ITestWidget::State, &ITestWidget::State::width>::createRef(&state);
    ASSERT_TRUE(ref);
    float val = 0.f;
    ref->get_data(&val, sizeof(float), type_uid<float>());
    EXPECT_FLOAT_EQ(val, 42.f);

    // Writing through the ref should update the state
    float newVal = 99.f;
    ref->set_data(&newVal, sizeof(float), type_uid<float>());
    EXPECT_FLOAT_EQ(state.width, 99.f);
}

TEST_F(ObjectTest, FnRawHasNoArgMetadata)
{
    auto* info = instance().type_registry().get_class_info(TestWidget::class_id());
    ASSERT_NE(info, nullptr);

    const MemberDesc* processDesc = nullptr;
    for (auto& m : info->members) {
        if (m.name == "process") { processDesc = &m; break; }
    }
    ASSERT_NE(processDesc, nullptr);

    auto* fk = processDesc->functionKind();
    ASSERT_NE(fk, nullptr);
    EXPECT_TRUE(fk->args.empty());
}

// --- RPROP tests ---

TEST_F(ObjectTest, RPropGetValueReturnsDefault)
{
    auto obj = instance().create<IObject>(TestWidget::class_id());
    auto* iw = interface_cast<ITestWidget>(obj);
    ASSERT_NE(iw, nullptr);

    // RPROP accessor returns ConstProperty<int>
    ConstProperty<int> id = iw->id();
    EXPECT_TRUE(id);
    EXPECT_EQ(id.get_value(), 42);
}

TEST_F(ObjectTest, RPropStateWriteReadable)
{
    auto obj = instance().create<IObject>(TestWidget::class_id());
    auto* iw = interface_cast<ITestWidget>(obj);
    auto* ps = interface_cast<IPropertyState>(obj);
    ASSERT_NE(iw, nullptr);
    ASSERT_NE(ps, nullptr);

    auto* state = ps->get_property_state<ITestWidget>();
    ASSERT_NE(state, nullptr);

    // Default in state struct
    EXPECT_EQ(state->id, 42);

    // Write to state directly, read through accessor
    state->id = 99;
    EXPECT_EQ(iw->id().get_value(), 99);
}

TEST_F(ObjectTest, RPropSetValueReturnsReadOnly)
{
    auto obj = instance().create<IObject>(TestWidget::class_id());
    auto* meta = interface_cast<IMetadata>(obj);
    ASSERT_NE(meta, nullptr);

    auto prop = meta->get_property("id");
    ASSERT_TRUE(prop);

    // set_value on a read-only property should return READ_ONLY
    Any<int> newVal(100);
    EXPECT_EQ(prop->set_value(*static_cast<const IAny*>(newVal)), ReturnValue::READ_ONLY);

    // Value should be unchanged
    auto* iw = interface_cast<ITestWidget>(obj);
    ASSERT_NE(iw, nullptr);
    EXPECT_EQ(iw->id().get_value(), 42);
}

TEST_F(ObjectTest, RPropSetDataReturnsReadOnly)
{
    auto obj = instance().create<IObject>(TestWidget::class_id());
    auto* meta = interface_cast<IMetadata>(obj);
    ASSERT_NE(meta, nullptr);

    auto prop = meta->get_property("id");
    ASSERT_TRUE(prop);

    auto* pi = interface_cast<IPropertyInternal>(prop);
    ASSERT_NE(pi, nullptr);

    // set_data on a read-only property should return READ_ONLY
    int newVal = 100;
    EXPECT_EQ(pi->set_data(&newVal, sizeof(int), type_uid<int>()), ReturnValue::READ_ONLY);

    // Value should be unchanged
    auto* iw = interface_cast<ITestWidget>(obj);
    ASSERT_NE(iw, nullptr);
    EXPECT_EQ(iw->id().get_value(), 42);
}

TEST_F(ObjectTest, RPropOnChangedObservable)
{
    auto obj = instance().create<IObject>(TestWidget::class_id());
    auto* iw = interface_cast<ITestWidget>(obj);
    auto* ps = interface_cast<IPropertyState>(obj);
    ASSERT_NE(iw, nullptr);
    ASSERT_NE(ps, nullptr);

    // RPROP should support add_on_changed for observing state changes
    int notified = 0;
    Callback onChange([&]() { notified++; });
    iw->id().add_on_changed(onChange);

    // Writing through state directly does not fire on_changed (by design),
    // but the accessor still works for observation setup
    EXPECT_EQ(notified, 0);
}

// --- User-specified class UID tests ---

class ITagged : public Interface<ITagged>
{
public:
    VELK_INTERFACE(
        (PROP, int, tag, 0)
    )
};

class TaggedWidget : public ext::Object<TaggedWidget, ITagged>
{
public:
    VELK_CLASS_UID("a0b1c2d3-e4f5-6789-abcd-ef0123456789");
};

class AutoUidWidget : public ext::Object<AutoUidWidget, ITagged>
{};

class ClassUidTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        instance().type_registry().register_type<TaggedWidget>();
        instance().type_registry().register_type<AutoUidWidget>();
    }
};

TEST_F(ClassUidTest, UserSpecifiedClassUid)
{
    constexpr Uid expected{"a0b1c2d3-e4f5-6789-abcd-ef0123456789"};
    static_assert(TaggedWidget::class_id() == expected, "User-specified class UID mismatch");
    EXPECT_EQ(TaggedWidget::class_id(), expected);
}

TEST_F(ClassUidTest, AutoGeneratedClassUid)
{
    EXPECT_EQ(AutoUidWidget::class_id(), type_uid<AutoUidWidget>());
    EXPECT_NE(AutoUidWidget::class_id(), TaggedWidget::class_id());
}

TEST_F(ClassUidTest, CreateByUserSpecifiedUid)
{
    constexpr Uid uid{"a0b1c2d3-e4f5-6789-abcd-ef0123456789"};
    auto obj = instance().create<IObject>(uid);
    ASSERT_TRUE(obj);

    auto* tagged = interface_cast<ITagged>(obj);
    ASSERT_NE(tagged, nullptr);
}
