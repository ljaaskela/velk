#include <gtest/gtest.h>

#include <velk/api/velk.h>
#include <velk/ext/object.h>
#include <velk/ext/plugin.h>
#include <velk/interface/types.h>

using namespace velk;

// A minimal interface for plugin-registered types
class IPluginWidget : public Interface<IPluginWidget>
{
public:
    VELK_INTERFACE(
        (PROP, int, value, 0)
    )
};

class PluginWidget : public ext::Object<PluginWidget, IPluginWidget>
{
};

// A test plugin that registers PluginWidget
class TestPlugin : public ext::Plugin<TestPlugin>
{
public:
    VELK_CLASS_UID("a0000000-0000-0000-0000-000000000001");

    ReturnValue initialize(IVelk& velk) override
    {
        initCount++;
        return velk.type_registry().register_type<PluginWidget>();
    }

    ReturnValue shutdown(IVelk& velk) override
    {
        shutdownCount++;
        return ReturnValue::SUCCESS;
    }

    string_view get_name() const override { return "TestPlugin"; }

    int initCount = 0;
    int shutdownCount = 0;
};

// A plugin whose initialize() fails
class FailingPlugin : public ext::Plugin<FailingPlugin>
{
public:
    VELK_CLASS_UID("a0000000-0000-0000-0000-000000000002");

    ReturnValue initialize(IVelk&) override { return ReturnValue::FAIL; }
    ReturnValue shutdown(IVelk&) override { return ReturnValue::SUCCESS; }
};

class PluginTest : public ::testing::Test
{
protected:
    IVelk& velk_ = instance();
    IPlugin::Ptr plugin_ = interface_pointer_cast<IPlugin>(ext::make_object<TestPlugin>());
    TestPlugin* tp_ = static_cast<TestPlugin*>(plugin_.get());

    void TearDown() override
    {
        auto& reg = velk_.plugin_registry();
        if (reg.find_plugin<TestPlugin>()) {
            reg.unload_plugin<TestPlugin>();
        }
        if (reg.find_plugin<FailingPlugin>()) {
            reg.unload_plugin<FailingPlugin>();
        }
    }
};

TEST_F(PluginTest, LoadPlugin)
{
    auto& reg = velk_.plugin_registry();

    ASSERT_EQ(ReturnValue::SUCCESS, reg.load_plugin(plugin_));
    EXPECT_EQ(1, tp_->initCount);
    EXPECT_EQ(0, tp_->shutdownCount);
}

TEST_F(PluginTest, FindPlugin)
{
    auto& reg = velk_.plugin_registry();

    reg.load_plugin(plugin_);
    EXPECT_EQ(plugin_.get(), reg.find_plugin<TestPlugin>());
    EXPECT_EQ(nullptr, reg.find_plugin(Uid{"ffffffff-ffff-ffff-ffff-ffffffffffff"}));
}

TEST_F(PluginTest, PluginCount)
{
    auto& reg = velk_.plugin_registry();

    EXPECT_EQ(0u, reg.plugin_count());
    reg.load_plugin(plugin_);
    EXPECT_EQ(1u, reg.plugin_count());
}

TEST_F(PluginTest, PluginTypesRegistered)
{
    auto& reg = velk_.plugin_registry();

    reg.load_plugin(plugin_);

    auto obj = velk_.create<IObject>(PluginWidget::class_id());
    ASSERT_TRUE(obj);
}

TEST_F(PluginTest, UnloadPlugin)
{
    auto& reg = velk_.plugin_registry();

    reg.load_plugin(plugin_);
    ASSERT_EQ(ReturnValue::SUCCESS, reg.unload_plugin<TestPlugin>());
    EXPECT_EQ(1, tp_->shutdownCount);
    EXPECT_EQ(nullptr, reg.find_plugin<TestPlugin>());
    EXPECT_EQ(0u, reg.plugin_count());
}

TEST_F(PluginTest, UnloadAutoUnregistersTypes)
{
    auto& reg = velk_.plugin_registry();

    reg.load_plugin(plugin_);

    ASSERT_NE(nullptr, velk_.type_registry().get_class_info(PluginWidget::class_id()));

    reg.unload_plugin<TestPlugin>();

    EXPECT_EQ(nullptr, velk_.type_registry().get_class_info(PluginWidget::class_id()));
}

TEST_F(PluginTest, DoubleLoadReturnsError)
{
    auto& reg = velk_.plugin_registry();

    ASSERT_EQ(ReturnValue::SUCCESS, reg.load_plugin(plugin_));
    EXPECT_EQ(ReturnValue::INVALID_ARGUMENT, reg.load_plugin(plugin_));
    EXPECT_EQ(1, tp_->initCount);
}

TEST_F(PluginTest, UnloadUnknownReturnsError)
{
    auto& reg = velk_.plugin_registry();
    EXPECT_EQ(ReturnValue::INVALID_ARGUMENT, reg.unload_plugin(Uid{"ffffffff-ffff-ffff-ffff-ffffffffffff"}));
}

TEST_F(PluginTest, BuiltinTypesSurvivePluginUnload)
{
    auto& reg = velk_.plugin_registry();

    reg.load_plugin(plugin_);
    reg.unload_plugin<TestPlugin>();

    auto prop = velk_.create_property<int>();
    ASSERT_TRUE(prop);
}

TEST_F(PluginTest, FailedInitializeDoesNotLoad)
{
    auto& reg = velk_.plugin_registry();
    auto fp = interface_pointer_cast<IPlugin>(ext::make_object<FailingPlugin>());

    EXPECT_EQ(ReturnValue::FAIL, reg.load_plugin(fp));
    EXPECT_EQ(nullptr, reg.find_plugin<FailingPlugin>());
    EXPECT_EQ(0u, reg.plugin_count());
}
