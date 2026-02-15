#include <gtest/gtest.h>

#include <api/any.h>
#include <api/function.h>
#include <api/function_context.h>
#include <api/property.h>
#include <api/strata.h>
#include <ext/object.h>
#include <interface/intf_event.h>
#include <interface/intf_metadata.h>
#include <interface/intf_property.h>
#include <interface/types.h>

using namespace strata;

// --- Test interfaces and implementation ---

class ITestWidget : public Interface<ITestWidget>
{
public:
    STRATA_INTERFACE(
        (PROP, float, width, 100.f),
        (PROP, float, height, 50.f),
        (EVT, on_clicked),
        (FN, reset)
    )
};

class ITestSerializable : public Interface<ITestSerializable>
{
public:
    STRATA_INTERFACE(
        (PROP, int, version, 1),
        (FN, serialize)
    )
};

class ITestMath : public Interface<ITestMath>
{
public:
    STRATA_INTERFACE(
        (FN, add, (int, x), (int, y))
    )
};

class ITestRaw : public Interface<ITestRaw>
{
public:
    STRATA_INTERFACE(
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

    ReturnValue fn_reset() override
    {
        resetCallCount++;
        return ReturnValue::SUCCESS;
    }

    ReturnValue fn_serialize() override
    {
        serializeCallCount++;
        return ReturnValue::SUCCESS;
    }

    ReturnValue fn_add(int x, int y) override
    {
        lastAddResult = x + y;
        return ReturnValue::SUCCESS;
    }

    ReturnValue fn_process(FnArgs) override
    {
        processCallCount++;
        return ReturnValue::SUCCESS;
    }
};

// --- Fixture to register once ---

class ObjectTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        instance().register_type<TestWidget>();
    }
};

// --- Tests ---

TEST_F(ObjectTest, RegisterAndCreate)
{
    auto obj = instance().create<IObject>(TestWidget::get_class_uid());
    ASSERT_TRUE(obj);
}

TEST_F(ObjectTest, InterfaceCastSucceeds)
{
    auto obj = instance().create<IObject>(TestWidget::get_class_uid());
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
    // IStrata is not implemented by TestWidget
    auto obj = instance().create<IObject>(TestWidget::get_class_uid());
    ASSERT_TRUE(obj);

    auto* bad = interface_cast<IStrata>(obj);
    EXPECT_EQ(bad, nullptr);
}

TEST_F(ObjectTest, MetadataGetPropertyByName)
{
    auto obj = instance().create<IObject>(TestWidget::get_class_uid());
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
    auto obj = instance().create<IObject>(TestWidget::get_class_uid());
    auto* meta = interface_cast<IMetadata>(obj);
    ASSERT_NE(meta, nullptr);

    auto bogus = meta->get_property("nonexistent");
    EXPECT_FALSE(bogus);
}

TEST_F(ObjectTest, MetadataGetEventByName)
{
    auto obj = instance().create<IObject>(TestWidget::get_class_uid());
    auto* meta = interface_cast<IMetadata>(obj);
    ASSERT_NE(meta, nullptr);

    auto event = meta->get_event("on_clicked");
    EXPECT_TRUE(event);
}

TEST_F(ObjectTest, MetadataGetFunctionByName)
{
    auto obj = instance().create<IObject>(TestWidget::get_class_uid());
    auto* meta = interface_cast<IMetadata>(obj);
    ASSERT_NE(meta, nullptr);

    auto fn = meta->get_function("reset");
    EXPECT_TRUE(fn);

    auto fn2 = meta->get_function("serialize");
    EXPECT_TRUE(fn2);
}

TEST_F(ObjectTest, PropertyDefaultsFromInterface)
{
    auto obj = instance().create<IObject>(TestWidget::get_class_uid());
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
    auto obj = instance().create<IObject>(TestWidget::get_class_uid());
    auto* iw = interface_cast<ITestWidget>(obj);
    ASSERT_NE(iw, nullptr);

    iw->width().set_value(42.f);
    EXPECT_FLOAT_EQ(iw->width().get_value(), 42.f);
}

TEST_F(ObjectTest, FunctionInvokeViaInterface)
{
    auto obj = instance().create<IObject>(TestWidget::get_class_uid());
    auto* iw = interface_cast<ITestWidget>(obj);
    ASSERT_NE(iw, nullptr);

    invoke_function(iw->reset());
    // Verify the virtual fn_reset was called (via the raw pointer)
    auto* raw = dynamic_cast<TestWidget*>(obj.get());
    ASSERT_NE(raw, nullptr);
    EXPECT_EQ(raw->resetCallCount, 1);
}

TEST_F(ObjectTest, FunctionInvokeByName)
{
    auto obj = instance().create<IObject>(TestWidget::get_class_uid());
    invoke_function(obj.get(), "reset");

    auto* raw = dynamic_cast<TestWidget*>(obj.get());
    ASSERT_NE(raw, nullptr);
    EXPECT_EQ(raw->resetCallCount, 1);
}

TEST_F(ObjectTest, StaticMetadataViaGetClassInfo)
{
    auto* info = instance().get_class_info(TestWidget::get_class_uid());
    ASSERT_NE(info, nullptr);

    // ITestWidget: width, height, on_clicked, reset
    // ITestSerializable: version, serialize
    // ITestMath: add
    // ITestRaw: process
    EXPECT_EQ(info->members.size(), 8u);

    // Check first member
    EXPECT_EQ(info->members[0].name, "width");
    EXPECT_EQ(info->members[0].kind, MemberKind::Property);

    EXPECT_EQ(info->members[2].name, "on_clicked");
    EXPECT_EQ(info->members[2].kind, MemberKind::Event);

    EXPECT_EQ(info->members[3].name, "reset");
    EXPECT_EQ(info->members[3].kind, MemberKind::Function);
}

TEST_F(ObjectTest, StaticDefaultValues)
{
    auto* info = instance().get_class_info(TestWidget::get_class_uid());
    ASSERT_NE(info, nullptr);

    EXPECT_FLOAT_EQ(get_default_value<float>(info->members[0]), 100.f);  // width
    EXPECT_FLOAT_EQ(get_default_value<float>(info->members[1]), 50.f);   // height
    EXPECT_EQ(get_default_value<int>(info->members[4]), 1);              // version
}

TEST_F(ObjectTest, PropertyStateReadWrite)
{
    auto obj = instance().create<IObject>(TestWidget::get_class_uid());
    auto* iw = interface_cast<ITestWidget>(obj);
    auto* ps = interface_cast<IPropertyState>(obj);
    ASSERT_NE(iw, nullptr);
    ASSERT_NE(ps, nullptr);

    auto* state = ps->get_property_state<ITestWidget>();
    ASSERT_NE(state, nullptr);

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
    auto obj = instance().create<IObject>(TestWidget::get_class_uid());
    auto* raw = dynamic_cast<TestWidget*>(obj.get());
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

TEST_F(ObjectTest, TypedFunctionArgMetadata)
{
    auto* info = instance().get_class_info(TestWidget::get_class_uid());
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
    auto* info = instance().get_class_info(TestWidget::get_class_uid());
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
    auto obj = instance().create<IObject>(TestWidget::get_class_uid());
    auto* raw = dynamic_cast<TestWidget*>(obj.get());
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
    auto* info = instance().get_class_info(TestWidget::get_class_uid());
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
